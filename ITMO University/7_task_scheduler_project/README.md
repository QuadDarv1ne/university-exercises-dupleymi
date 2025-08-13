# TTaskScheduler — шедулер задач с зависимостями (C++)

Проект демонстрирует реализацию простого шедулера задач **TTaskScheduler**, который позволяет:

- Добавлять задачи (любой callable: функции, лямбды, указатели на методы классов).
- Задавать зависимости между задачами через объект `FutureResult<T>` (результат другой задачи).
- Поддерживать до 2 аргументов у задачи.
- Ленивая оценка: `getResult<T>(id)` вычисляет только необходимые задачи.
- Принудительное выполнение всех задач через `executeAll()`.
- Обнаружение циклических зависимостей (бросается `std::runtime_error`).

## Файлы в репозитории

- `task_scheduler.hpp` — заголовочный файл с реализацией и Doxygen-совместимыми DocString'ами (на русском).
- `tests.cpp` — тесты на Google Test, демонстрирующие основные сценарии (квадратное уравнение, ленивое исполнение, цикл, вызов метода класса).
- `CMakeLists.txt` — примерный CMake-файл для сборки тестов (требует установленный GoogleTest).
- `README.md` — этот файл.
- `build_log.txt` — лог автоматической попытки сборки (в корне архива), если вы загружаете архив, посмотрите его при проблемах.

## Быстрая инструкция по сборке и запуску тестов (Linux / macOS)

### Вариант A — с установленным GoogleTest (рекомендуется для систем, где пакет доступен)

1. Установите зависимости (пример для Ubuntu/Debian):

```bash
sudo apt update
sudo apt install -y build-essential cmake libgtest-dev
```

> Примечание: на некоторых дистрибутивах пакет `libgtest-dev` содержит исходники, и потребуется собрать их отдельно. В этом случае предпочтительнее использовать FetchContent или скачать GoogleTest вручную.

2. Создайте директорию сборки и соберите:

```bash
mkdir -p build
cd build
cmake ..
cmake --build . -- -j
```

3. Запустите тесты:

```bash
./tests
```

### Вариант B — использование FetchContent для автоматического скачивания GoogleTest

Если у вас нет GoogleTest в системе, рекомендуем изменить `CMakeLists.txt`, добавив `FetchContent` для gtest, либо выполните локальную замену. Пример minimal `CMakeLists.txt` со FetchContent:

```cmake
cmake_minimum_required(VERSION 3.14)
project(TaskSchedulerTests CXX)
set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-1.15.0
)
FetchContent_MakeAvailable(googletest)

add_executable(tests tests.cpp)
target_link_libraries(tests PRIVATE GTest::gtest_main)
target_include_directories(tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
```

После этого:

```bash
mkdir build && cd build
cmake ..
cmake --build . -- -j
./tests
```

## Пример использования (в коде)

См. `tests.cpp` — там показан пример вычисления корней квадратного уравнения и использование метода класса как callable.

## Примечания по реализации

- Результаты хранятся в `AnyValue` (type-erasure), поэтому `getResult<T>(id)` проверяет соответствие типа и бросает `std::runtime_error`, если тип неожидан.
- Арность задач ограничена до 2 — это требование лабораторной работы.
- Данный пример фокусируется на понятности и демонстрации концепции; для промышленного использования стоит улучшить обработку ошибок, сообщения об ошибках и покрытие краёвых случаев.

## Лицензия

MIT — вы можете свободно использовать, модифицировать и распространять код.