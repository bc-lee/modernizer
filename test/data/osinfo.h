#ifndef OSINFO_H_
#define OSINFO_H_

#include "rtc_base/constructor_magic.h"

class OSInfo {
 public:
  static OSInfo* GetInstance();

 private:
  OSInfo();
  ~OSInfo();

  RTC_DISALLOW_COPY_AND_ASSIGN(OSInfo);
};

#endif  // OSINFO_H_
