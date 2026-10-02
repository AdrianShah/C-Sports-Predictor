// A deliberately tiny test harness: TEST_CASE registers a function, CHECK*
// macros record failures without stopping the test.
#pragma once

#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace check {

struct Case {
  const char* name;
  void (*fn)();
};

inline std::vector<Case>& registry() {
  static std::vector<Case> cases;
  return cases;
}

inline int& failures() {
  static int count = 0;
  return count;
}

struct Register {
  Register(const char* name, void (*fn)()) { registry().push_back({name, fn}); }
};

inline void fail(const char* file, int line, const std::string& what) {
  ++failures();
  std::cerr << file << ':' << line << ": check failed: " << what << '\n';
}

}  // namespace check

#define TEST_CASE(name)                                          \
  static void name();                                            \
  static const ::check::Register name##_registration{#name, name}; \
  static void name()

#define CHECK(cond)                                           \
  do {                                                        \
    if (!(cond)) ::check::fail(__FILE__, __LINE__, #cond);    \
  } while (false)

// Copies both sides: (a) may be a reference into a temporary, e.g. opt().value().
#define CHECK_EQ(a, b)                                                           \
  do {                                                                           \
    const auto check_a_ = (a);                                                   \
    const auto check_b_ = (b);                                                   \
    if (!(check_a_ == check_b_)) {                                               \
      std::ostringstream check_os_;                                              \
      check_os_ << #a " == " #b " (" << check_a_ << " vs " << check_b_ << ")";   \
      ::check::fail(__FILE__, __LINE__, check_os_.str());                        \
    }                                                                            \
  } while (false)

#define CHECK_NEAR(a, b, eps)                                                        \
  do {                                                                               \
    const double check_a_ = (a);                                                     \
    const double check_b_ = (b);                                                     \
    if (!(std::abs(check_a_ - check_b_) <= (eps))) {                                 \
      std::ostringstream check_os_;                                                  \
      check_os_ << #a " ~= " #b " (" << check_a_ << " vs " << check_b_ << ")";       \
      ::check::fail(__FILE__, __LINE__, check_os_.str());                            \
    }                                                                                \
  } while (false)

#define CHECK_THROWS(expr, Exception)                                         \
  do {                                                                        \
    bool check_thrown_ = false;                                               \
    try {                                                                     \
      static_cast<void>(expr);                                                \
    } catch (const Exception&) {                                              \
      check_thrown_ = true;                                                   \
    } catch (...) {                                                           \
    }                                                                         \
    if (!check_thrown_) ::check::fail(__FILE__, __LINE__, "expected " #Exception " from " #expr); \
  } while (false)
