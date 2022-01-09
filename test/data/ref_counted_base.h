#ifndef REF_COUNTED_BASE_H_
#define REF_COUNTED_BASE_H_

#include <atomic>

#include "rtc_base/constructor_magic.h"

enum class RefCountReleaseStatus { kDroppedLastRef, kOtherRefsRemained };

class RefCountedBase {
 public:
  RefCountedBase() = default;

  void AddRef();
  RefCountReleaseStatus Release();

 protected:
  bool HasOneRef() const { return ref_count_.load() == 1; }

  virtual ~RefCountedBase() = default;

 private:
  std::atomic<int> ref_count_{0};

  RTC_DISALLOW_COPY_AND_ASSIGN(RefCountedBase);
};

#endif  // REF_COUNTED_BASE_H_
