#ifndef REF_COUNTED_BASE_H_
#define REF_COUNTED_BASE_H_

#include <atomic>

enum class RefCountReleaseStatus { kDroppedLastRef, kOtherRefsRemained };

class RefCountedBase {
 public:
  RefCountedBase() = default;

  RefCountedBase(const RefCountedBase&) = delete;
  RefCountedBase& operator=(const RefCountedBase&) = delete;

  void AddRef();
  RefCountReleaseStatus Release();

 protected:
  bool HasOneRef() const { return ref_count_.load() == 1; }

  virtual ~RefCountedBase() = default;

 private:
  std::atomic<int> ref_count_{0};
};

#endif  // REF_COUNTED_BASE_H_
