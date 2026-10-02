#include "picks/strings.hpp"

namespace picks {

std::string to_lower_ascii(std::string_view s) {
  std::string out(s);
  for (char& c : out) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  }
  return out;
}

bool contains_ci(std::string_view haystack, std::string_view needle) {
  return to_lower_ascii(haystack).find(to_lower_ascii(needle)) != std::string::npos;
}

}  // namespace picks
