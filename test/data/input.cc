#include <string>

#include "audio_encoder.h"
#include "byte_buffer.h"
#include "data_encoding.h"
#include "mutex_lock.h"
#include "osinfo.h"
#include "ref_counted_base.h"
#include "rtc_base/constructor_magic.h"

namespace {

class Foo {
 public:
  Foo();
  ~Foo() = default;

  std::string foo();

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(Foo);
  std::string foo_{"foo"};
};

std::string Foo::foo() {
  return foo_;
}

class Barrrrrrrrr {
 public:
  Barrrrrrrrr();
  ~Barrrrrrrrr() { bar = 0; }

  int barrr() { return bar++; }

  int bar = 0;

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(Barrrrrrrrr);
};
}  // namespace
