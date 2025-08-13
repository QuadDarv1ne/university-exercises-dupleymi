/**
 * 1) QuadraticExample — Базовый пример решения квадратного уравнения
 * Тест проверяет корректность выполнения цепочки зависимых задач, моделирующих решение квадратного уравнения. Также проверяется вызов метода класса через TTaskScheduler.
 * 
 * 2) LazyEvaluation — Ленивая оценка задач
 * Проверяет, что задачи выполняются только тогда, когда требуется их результат. Задачи, не участвующие в вычислениях, не должны выполняться.
 * 
 * 3) DetectCycle — Обнаружение цикла зависимостей
 * Тест проверяет корректное выявление циклических зависимостей между задачами. Если возникает цикл, getResult должен выбросить исключение.
 * 
 * 4) MemberFunctionCall — Вызов метода класса
 * Проверяет возможность добавления задачи, которая вызывает метод объекта класса с параметром из другой задачи.
 * 
 * 5) MultipleConsumersShareSingleProducer — Один продюсер — несколько потребителей
 * Тестирует, что несколько задач, зависящих от одной исходной, используют общий результат и исходная задача вычисляется только один раз.
 * 
 * 6) ArgumentsOrderIsPreserved — Проверка порядка аргументов
 * Убедиться, что порядок передачи аргументов (значение + future или future + значение) сохраняется и вычисляется правильно.
 * 
 * 7) ExecuteAllRunsEveryTask — executeAll() выполняет все задачи
 * Проверяет работу метода executeAll(), который должен выполнить все задачи независимо от их зависимости, а также убедиться, что результаты кешируются.
 * 
 * 8) TypeMismatchThrows — Неверный тип результата
 * Проверяет, что попытка получить результат с типом, не совпадающим с типом задачи, вызывает исключение. Верный тип должен работать корректно.
 * 
 * 9) DeepDependencyChain — Глубокая цепочка зависимостей
 * Тестирует корректность работы с длинными цепочками зависимых задач. Проверяется рекурсивное вычисление и кеширование результатов на всех уровнях.
 */

#include "task_scheduler.hpp"
#include <gtest/gtest.h>
#include <cmath>

// Структура для проверки вызова метода класса
struct AddNumber {
  float number;
  float add(float a) const {
    return a + number;
  }
};

// 1) Базовый пример — квадратное уравнение.
TEST(TaskScheduler, QuadraticExample) {
  TTaskScheduler sched;

  float a = 1.0f;
  float b = -2.0f;
  float c = 1.0f;
  AddNumber ad{3.0f};

  auto id1 = sched.add([](float a, float c) { return -4.0f * a * c; }, a, c);
  auto id2 = sched.add([](float b, float v) { return b * b + v; }, b, sched.getFutureResult<float>(id1));
  auto id3 = sched.add([](float b, float d) { return -b + std::sqrt(d); }, b, sched.getFutureResult<float>(id2));
  auto id4 = sched.add([](float b, float d) { return -b - std::sqrt(d); }, b, sched.getFutureResult<float>(id2));
  auto id5 = sched.add([](float a, float v) { return v / (2.0f * a); }, a, sched.getFutureResult<float>(id3));
  auto id6 = sched.add([](float a, float v) { return v / (2.0f * a); }, a, sched.getFutureResult<float>(id4));
  auto id7 = sched.add(&AddNumber::add, ad, sched.getFutureResult<float>(id6));

  float x1 = sched.getResult<float>(id5);
  float x2 = sched.getResult<float>(id6);
  float x3 = sched.getResult<float>(id7);

  EXPECT_NEAR(x1, 1.0f, 1e-6f);
  EXPECT_NEAR(x2, 1.0f, 1e-6f);
  EXPECT_NEAR(x3, 4.0f, 1e-6f);
}

// 2) Ленивая оценка
TEST(TaskScheduler, LazyEvaluation) {
  TTaskScheduler sched;

  int counter = 0;

  auto id0 = sched.add([&counter]() {
    ++counter;
    return 10;
  });

  auto id1 = sched.add([](int x) { return x + 1; }, sched.getFutureResult<int>(id0));

  auto id2 = sched.add([&counter]() { // не должен выполниться
    ++counter;
    return 100;
  });

  int r = sched.getResult<int>(id1);
  EXPECT_EQ(r, 11);
  EXPECT_EQ(counter, 1); // только id0 выполнился
}

// 3) Обнаружение цикла
TEST(TaskScheduler, DetectCycle) {
  TTaskScheduler sched;

  // создаём цикл: id0 -> id1 -> id0
  auto id0 = sched.add([](int x) { return x + 1; }, sched.getFutureResult<int>(1));
  auto id1 = sched.add([](int x) { return x + 2; }, sched.getFutureResult<int>(0));

  EXPECT_THROW(sched.getResult<int>(id0), std::runtime_error);
}

// 4) Вызов метода класса
TEST(TaskScheduler, MemberFunctionCall) {
  TTaskScheduler sched;

  struct M {
    int offset;
    int addOne(int x) const { return x + offset; }
  };

  M m{5};

  auto id0 = sched.add([]() { return 42; });
  auto id1 = sched.add(&M::addOne, m, sched.getFutureResult<int>(id0));

  int r = sched.getResult<int>(id1);
  EXPECT_EQ(r, 47);
}

/* -------------------- Дополнительные тесты -------------------- */

// 5) Один продюсер — несколько потребителей: продюсер вычисляется один раз.
TEST(TaskScheduler, MultipleConsumersShareSingleProducer) {
  TTaskScheduler sched;

  int counter = 0;
  auto id0 = sched.add([&counter]() {
    ++counter;
    return 5;
  });

  auto id1 = sched.add([](int x) { return x * 2; }, sched.getFutureResult<int>(id0));
  auto id2 = sched.add([](int x) { return x + 7; }, sched.getFutureResult<int>(id0));

  int r1 = sched.getResult<int>(id1);
  int r2 = sched.getResult<int>(id2);

  EXPECT_EQ(r1, 10);
  EXPECT_EQ(r2, 12);
  EXPECT_EQ(counter, 1); // источник вычислился ровно один раз
}

// 6) Проверка порядка аргументов (value + future) и (future + value).
TEST(TaskScheduler, ArgumentsOrderIsPreserved) {
  TTaskScheduler sched;

  auto id0 = sched.add([]() { return 3; });

  // f(a, future) = a - future
  auto id1 = sched.add([](int a, int b) { return a - b; }, 10, sched.getFutureResult<int>(id0));
  // f(future, a) = future - a
  auto id2 = sched.add([](int a, int b) { return a - b; }, sched.getFutureResult<int>(id0), 10);

  int r1 = sched.getResult<int>(id1);
  int r2 = sched.getResult<int>(id2);

  EXPECT_EQ(r1, 7);   // 10 - 3
  EXPECT_EQ(r2, -7);  // 3 - 10
}

// 7) executeAll() действительно выполняет все задачи.
TEST(TaskScheduler, ExecuteAllRunsEveryTask) {
  TTaskScheduler sched;

  int c0 = 0, c1 = 0, c2 = 0;

  auto id0 = sched.add([&]() { ++c0; return 1; });
  auto id1 = sched.add([&](int x) { ++c1; return x + 1; }, sched.getFutureResult<int>(id0));
  auto id2 = sched.add([&](int x) { ++c2; return x * 10; }, sched.getFutureResult<int>(id1));

  // До вызова executeAll ничего не посчитано.
  EXPECT_EQ(c0, 0);
  EXPECT_EQ(c1, 0);
  EXPECT_EQ(c2, 0);

  sched.executeAll();

  // Все три задачи должны были выполниться
  EXPECT_EQ(c0, 1);
  EXPECT_EQ(c1, 1);
  EXPECT_EQ(c2, 1);

  // И результаты доступны без повторного вычисления
  EXPECT_EQ(sched.getResult<int>(id2), 20);
  EXPECT_EQ(c0, 1);
  EXPECT_EQ(c1, 1);
  EXPECT_EQ(c2, 1);
}

// 8) Неверный тип в getResult<T>() должен кидать исключение.
TEST(TaskScheduler, TypeMismatchThrows) {
  TTaskScheduler sched;

  auto id = sched.add([]() { return 42; });

  EXPECT_THROW(sched.getResult<float>(id), std::runtime_error);
  // А с верным типом — всё ок
  EXPECT_NO_THROW({
    int v = sched.getResult<int>(id);
    (void)v;
  });
}

// 9) Глубокая цепочка зависимостей: проверяем рекурсию и кеширование.
TEST(TaskScheduler, DeepDependencyChain) {
  TTaskScheduler sched;

  auto id0 = sched.add([]() { return 0; });
  auto id1 = sched.add([](int x) { return x + 1; }, sched.getFutureResult<int>(id0));
  auto id2 = sched.add([](int x) { return x + 1; }, sched.getFutureResult<int>(id1));
  auto id3 = sched.add([](int x) { return x + 1; }, sched.getFutureResult<int>(id2));
  auto id4 = sched.add([](int x) { return x + 1; }, sched.getFutureResult<int>(id3));
  auto id5 = sched.add([](int x) { return x + 1; }, sched.getFutureResult<int>(id4));

  int r = sched.getResult<int>(id5);
  EXPECT_EQ(r, 5);
}
