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

}  // namespace
