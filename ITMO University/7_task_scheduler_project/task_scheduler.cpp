#include "task_scheduler.hpp"

// Глобальный указатель для доступа в unwrap_future
// Устанавливается при вычислении
const TTaskScheduler* g_currentScheduler = nullptr;
