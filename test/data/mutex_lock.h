#ifndef MUTEX_LOCK_H_
#define MUTEX_LOCK_H_

#include <mutex>

#include "rtc_base/constructor_magic.h"

#define SCOPED_LOCKABLE __attribute__((scoped_lockable))
#define EXCLUSIVE_LOCK_FUNCTION(...) \
  __attribute__((exclusive_lock_function(__VA_ARGS__)))
#define UNLOCK_FUNCTION(...) __attribute__((unlock_function(__VA_ARGS__)))

class SCOPED_LOCKABLE MutexLock final {
 public:
  explicit MutexLock(std::mutex& mutex) EXCLUSIVE_LOCK_FUNCTION(mutex);
  ~MutexLock() UNLOCK_FUNCTION();

 private:
  std::mutex& mutex_;
  RTC_DISALLOW_COPY_AND_ASSIGN(MutexLock);
};

#endif  // MUTEX_LOCK_H_
