#include <string>

#include "rtc_base/constructor_magic.h"

namespace {

class Foo {
 public:
  Foo();
  ~Foo();

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
