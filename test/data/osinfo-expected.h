#ifndef OSINFO_H_
#define OSINFO_H_

#include "rtc_base/constructor_magic.h"

class OSInfo {
 public:
  static OSInfo* GetInstance();

 private:
  OSInfo();
  ~OSInfo();

  OSInfo(const OSInfo&) = delete;
  OSInfo& operator=(const OSInfo&) = delete;
};

#endif  // OSINFO_H_
