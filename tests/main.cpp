#include <exception>
#include <iostream>
#include <string_view>

#include "check.hpp"

// Usage: picks_tests [substring]  (runs only tests whose name contains it)
int main(int argc, char** argv) {
  const std::string_view filter = argc > 1 ? argv[1] : "";
  int run = 0;
  int failed_tests = 0;
  for (const auto& test : check::registry()) {
    if (!filter.empty() && std::string_view(test.name).find(filter) == std::string_view::npos) continue;
    ++run;
    const int before = check::failures();
    try {
      test.fn();
    } catch (const std::exception& e) {
      check::fail(test.name, 0, std::string("unexpected exception: ") + e.what());
    }
    const bool ok = check::failures() == before;
    if (!ok) ++failed_tests;
    std::cout << (ok ? "  ok    " : "  FAIL  ") << test.name << '\n';
  }
  std::cout << run << " tests, " << failed_tests << " failed\n";
  return failed_tests == 0 && run > 0 ? 0 : 1;
}
