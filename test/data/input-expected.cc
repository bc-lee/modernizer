#include <string>

#include "mutex_lock.h"
#include "rtc_base/constructor_magic.h"

namespace {

class Foo {
 public:
  Foo();
  ~Foo();

  Foo(const Foo&) = delete;
  Foo& operator=(const Foo&) = delete;

  std::string foo();

 private:
  std::string foo_{"foo"};
};

std::string Foo::foo() {
  return foo_;
}

class Barrrrrrrrr {
 public:
  Barrrrrrrrr();
  ~Barrrrrrrrr() { bar = 0; }

  Barrrrrrrrr(const Barrrrrrrrr&) = delete;
  Barrrrrrrrr& operator=(const Barrrrrrrrr&) = delete;

  int barrr() { return bar++; }

  int bar = 0;
};
}  // namespace
