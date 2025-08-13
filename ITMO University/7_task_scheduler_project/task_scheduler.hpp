#ifndef TASK_SCHEDULER_HPP
#define TASK_SCHEDULER_HPP

#include <vector>
#include <memory>
#include <tuple>
#include <utility>
#include <stdexcept>
#include <type_traits>
#include <functional>
#include <cmath>
#include <memory>

/**
 * @file task_scheduler.hpp
 * @brief Реализация TTaskScheduler — шедулера задач с зависимостями результатов.
 *
 * Документация на русском языке и Doxygen-совместимые DocString'и присутствуют в этом файле.
 */

/**
 * @class AnyValue
 * @brief Простая реализация type-erasure для хранения произвольного значения.
 *
 * Используется для хранения результатов задач разного типа в одном контейнере.
 */
class AnyValue {
  struct Base {
    virtual ~Base() = default;
  };

  template<typename T>
  struct Holder : Base {
    T value;
    template<typename U>
    Holder(U&& v) : value(std::forward<U>(v)) {}
  };

  std::shared_ptr<Base> ptr;

public:
  AnyValue() = default;

  template<typename T, typename = std::enable_if_t<!std::is_same<std::decay_t<T>, AnyValue>::value>>
  AnyValue(T&& v) : ptr(std::make_shared<Holder<std::decay_t<T>>>(std::forward<T>(v))) {}

  bool empty() const { return !ptr; }

  template<typename T>
  T* try_cast() {
    using H = Holder<std::decay_t<T>>;
    auto p = std::dynamic_pointer_cast<H>(ptr);
    return p ? &p->value : nullptr;
  }

  template<typename T>
  const T* try_cast() const {
    using H = Holder<std::decay_t<T>>;
    auto p = std::dynamic_pointer_cast<H>(ptr);
    return p ? &p->value : nullptr;
  }
};

/**
 * @struct FutureResult
 * @brief Маркер зависимости на результат задачи с указанным id.
 *
 * Используется при добавлении новой задачи, чтобы указать, что аргумент должен
 * браться из результата другой задачи.
 */
template<typename T>
struct FutureResult {
  size_t id;
  explicit FutureResult(size_t i = 0) : id(i) {}
};

/**
 * @brief Вспомогательный шаблон для разворачивания FutureResult<T> -> T.
 */
template<typename A>
struct unwrap_future { using type = std::decay_t<A>; };

template<typename T>
struct unwrap_future<FutureResult<T>> { using type = T; };

/**
 * @class TTaskScheduler
 * @brief Шедулер задач с поддержкой зависимостей по результатам других задач.
 *
 * TTaskScheduler позволяет добавлять задачи — любые callable-объекты (функции, лямбды,
 * указатели на методы классов). Каждая задача может принимать не более двух аргументов.
 * Аргументы могут быть обычными значениями (они копируются/перемещаются при добавлении)
 * либо специальными объектами FutureResult<T>, которые указывают на результат другой задачи
 * (идентифицируемой по id). Когда задача добавлена, она не выполняется сразу — выполнение
 * происходит лениво при вызове getResult<T>(id) или принудительно методом executeAll().
 *
 * Особенности:
 *  - Результаты хранятся в обёртке AnyValue (type-erasure).
 *  - При попытке получить результат с неверным типом будет выброшено std::runtime_error.
 *  - При наличии циклических зависимостей вычисление приводит к std::runtime_error.
 */
class TTaskScheduler {
public:
  TTaskScheduler() = default;

  template<typename Fnc, typename... Args>
  size_t add(Fnc&& f, Args&&... args) {
    static_assert(sizeof...(Args) <= 2, "Максимум 2 аргумента поддерживается");

    using DecayedTuple = std::tuple<typename unwrap_future<std::decay_t<Args>>::type...>;
    auto inputs = std::make_shared<std::tuple<InputDesc<typename unwrap_future<std::decay_t<Args>>::type>...>>(
      InputDesc<typename unwrap_future<std::decay_t<Args>>::type>(std::forward<Args>(args))...);

    auto exec = [f = std::forward<Fnc>(f), inputs](TTaskScheduler& sched) -> AnyValue {
      return TTaskScheduler::invoke_callable(f, inputs, sched);
    };

    tasks.emplace_back();
    tasks.back().executor = std::move(exec);
    visiting.resize(tasks.size(), false);
    return tasks.size() - 1;
  }

  template<typename T>
  FutureResult<T> getFutureResult(size_t id) const { return FutureResult<T>(id); }

  template<typename T>
  T getResult(size_t id) {
    if (id >= tasks.size()) throw std::out_of_range("Task id out of range");
    if (visiting.size() != tasks.size()) visiting.resize(tasks.size(), false);
    AnyValue av = computeInternal(id);
    T* p = av.try_cast<T>();
    if (!p) throw std::runtime_error("Bad result type requested in getResult");
    return *p;
  }

  void executeAll() {
    visiting.assign(tasks.size(), false);
    for (size_t i = 0; i < tasks.size(); ++i) {
      if (!tasks[i].evaluated) computeInternal(i);
    }
  }

  size_t size() const { return tasks.size(); }

private:
  template<typename T>
  struct InputDesc {
    bool isDep;
    size_t depId;
    T value;

    InputDesc(T&& v) : isDep(false), depId(static_cast<size_t>(-1)), value(std::forward<T>(v)) {}
    InputDesc(const T& v) : isDep(false), depId(static_cast<size_t>(-1)), value(v) {}
    InputDesc(FutureResult<T> f) : isDep(true), depId(f.id), value() {}

    T get(TTaskScheduler& s) const {
      if (!isDep) return value;
      return s.template getResult<T>(depId);
    }
  };

  struct Task {
    std::function<AnyValue(TTaskScheduler&)> executor;
    AnyValue result;
    bool evaluated = false;
  };

  std::vector<Task> tasks;
  std::vector<bool> visiting;

  AnyValue computeInternal(size_t id) {
    if (id >= tasks.size()) throw std::out_of_range("Task id out of range");
    if (tasks[id].evaluated) return tasks[id].result;
    if (visiting[id]) throw std::runtime_error("Cyclic dependency detected");
    visiting[id] = true;

    if (!tasks[id].executor) {
      visiting[id] = false;
      throw std::runtime_error("Task has no executor");
    }

    AnyValue res = tasks[id].executor(*this);
    tasks[id].result = std::move(res);
    tasks[id].evaluated = true;
    visiting[id] = false;
    return tasks[id].result;
  }

  template<typename Fnc, typename TuplePtr>
  static AnyValue invoke_callable(Fnc f, TuplePtr inputs_ptr, TTaskScheduler& sched) {
    constexpr size_t N = std::tuple_size<typename std::remove_reference<decltype(*inputs_ptr)>::type>::value;
    return invoke_impl(f, inputs_ptr, sched, std::integral_constant<size_t, N>{});
  }

  template<typename Fnc, typename TuplePtr>
  static AnyValue invoke_impl(Fnc& f, TuplePtr, TTaskScheduler&, std::integral_constant<size_t, 0>) {
    using R = std::invoke_result_t<Fnc>;
    if constexpr (std::is_void<R>::value) {
      std::invoke(f);
      return AnyValue();
    } else {
      R r = std::invoke(f);
      return AnyValue(std::move(r));
    }
  }

  template<typename Fnc, typename TuplePtr>
  static AnyValue invoke_impl(Fnc& f, TuplePtr inputs_ptr, TTaskScheduler& sched, std::integral_constant<size_t, 1>) {
    auto& tpl = *inputs_ptr;
    auto& a0 = std::get<0>(tpl);
    using V0 = decltype(a0.get(sched));
    V0 v0 = a0.get(sched);
    using R = std::invoke_result_t<Fnc, V0>;
    if constexpr (std::is_void<R>::value) {
      std::invoke(f, std::move(v0));
      return AnyValue();
    } else {
      R r = std::invoke(f, std::move(v0));
      return AnyValue(std::move(r));
    }
  }

  template<typename Fnc, typename TuplePtr>
  static AnyValue invoke_impl(Fnc& f, TuplePtr inputs_ptr, TTaskScheduler& sched, std::integral_constant<size_t, 2>) {
    auto& tpl = *inputs_ptr;
    auto& a0 = std::get<0>(tpl);
    auto& a1 = std::get<1>(tpl);
    using V0 = decltype(a0.get(sched));
    using V1 = decltype(a1.get(sched));
    V0 v0 = a0.get(sched);
    V1 v1 = a1.get(sched);
    using R = std::invoke_result_t<Fnc, V0, V1>;
    if constexpr (std::is_void<R>::value) {
      std::invoke(f, std::move(v0), std::move(v1));
      return AnyValue();
    } else {
      R r = std::invoke(f, std::move(v0), std::move(v1));
      return AnyValue(std::move(r));
    }
  }
};

#endif // TASK_SCHEDULER_HPP
