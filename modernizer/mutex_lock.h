#ifndef MODERNIZER_MUTEX_LOCK_H_
#define MODERNIZER_MUTEX_LOCK_H_

#include "absl/base/thread_annotations.h"

namespace modernizer {

template <class Mutex>
class SCOPED_LOCKABLE MutexLock final {
 public:
  explicit MutexLock(Mutex& mutex) EXCLUSIVE_LOCK_FUNCTION(mutex)
      : mutex_(mutex) {
    mutex.lock();
  }
  ~MutexLock() UNLOCK_FUNCTION() { mutex_.unlock(); }

  MutexLock(const MutexLock&) = delete;
  MutexLock& operator=(const MutexLock&) = delete;

 private:
  Mutex& mutex_;
};

}  // namespace modernizer

#endif  // MODERNIZER_MUTEX_LOCK_H_
