#pragma once

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace picks {

struct JsonError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

/// A small JSON value: enough to read fixtures.json and write model_picks.json.
/// Objects keep their key order so written files diff cleanly.
class Json {
 public:
  using Array = std::vector<Json>;
  using Object = std::vector<std::pair<std::string, Json>>;

  Json() noexcept : value_(nullptr) {}
  Json(std::nullptr_t) noexcept : value_(nullptr) {}
  Json(bool b) noexcept : value_(b) {}
  template <typename T>
    requires(std::is_arithmetic_v<T> && !std::is_same_v<T, bool>)
  Json(T number) noexcept : value_(static_cast<double>(number)) {}
  Json(const char* s) : value_(std::string(s)) {}
  Json(std::string s) noexcept : value_(std::move(s)) {}
  Json(std::string_view s) : value_(std::string(s)) {}
  Json(Array a) noexcept : value_(std::move(a)) {}
  Json(Object o) noexcept : value_(std::move(o)) {}

  /// Parses a complete document; throws JsonError with the byte offset on failure.
  static Json parse(std::string_view text);

  /// Serializes the value. indent < 0 gives compact output.
  std::string dump(int indent = -1) const;

  bool is_null() const noexcept { return std::holds_alternative<std::nullptr_t>(value_); }
  bool is_bool() const noexcept { return std::holds_alternative<bool>(value_); }
  bool is_number() const noexcept { return std::holds_alternative<double>(value_); }
  bool is_string() const noexcept { return std::holds_alternative<std::string>(value_); }
  bool is_array() const noexcept { return std::holds_alternative<Array>(value_); }
  bool is_object() const noexcept { return std::holds_alternative<Object>(value_); }

  bool as_bool() const { return get<bool>("bool"); }
  double as_number() const { return get<double>("number"); }
  const std::string& as_string() const { return get<std::string>("string"); }
  const Array& as_array() const { return get<Array>("array"); }
  const Object& as_object() const { return get<Object>("object"); }

  /// Member lookup; nullptr if this isn't an object or the key is missing.
  const Json* find(std::string_view key) const noexcept;
  /// The member as a string, or `fallback` if it's missing or not a string.
  std::string get_string(std::string_view key, std::string_view fallback = {}) const;
  std::optional<double> get_number(std::string_view key) const;

 private:
  template <typename T>
  const T& get(const char* what) const {
    if (const T* v = std::get_if<T>(&value_)) return *v;
    throw JsonError(std::string("JSON: expected ") + what);
  }

  void dump_to(std::string& out, int indent, int depth) const;

  std::variant<std::nullptr_t, bool, double, std::string, Array, Object> value_;
};

}  // namespace picks
