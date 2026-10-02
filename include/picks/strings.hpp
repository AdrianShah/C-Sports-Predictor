#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

namespace picks {

/// Transparent hash: maps keyed by std::string can be searched with a string_view
/// without allocating.
struct StringHash {
  using is_transparent = void;
  std::size_t operator()(std::string_view s) const noexcept {
    return std::hash<std::string_view>{}(s);
  }
};

/// Lowercases ASCII letters; other bytes (including UTF-8) are kept.
std::string to_lower_ascii(std::string_view s);

/// Case-insensitive (ASCII) substring search.
bool contains_ci(std::string_view haystack, std::string_view needle);

}  // namespace picks
