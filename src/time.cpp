#include "picks/time.hpp"

#include <cstdio>

namespace picks {
namespace {

bool read_digits(std::string_view s, std::size_t pos, std::size_t count, int& out) {
  if (pos + count > s.size()) return false;
  int value = 0;
  for (std::size_t i = 0; i < count; ++i) {
    const char c = s[pos + i];
    if (c < '0' || c > '9') return false;
    value = value * 10 + (c - '0');
  }
  out = value;
  return true;
}

bool is_leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

int days_in_month(int y, int m) {
  static constexpr int kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return m == 2 && is_leap(y) ? 29 : kDays[m - 1];
}

}  // namespace

std::optional<Timestamp> parse_iso8601(std::string_view s) {
  int year = 0, month = 0, day = 0;
  if (!read_digits(s, 0, 4, year) || s.size() < 10 || s[4] != '-' || !read_digits(s, 5, 2, month) ||
      s[7] != '-' || !read_digits(s, 8, 2, day)) {
    return std::nullopt;
  }
  if (month < 1 || month > 12 || day < 1 || day > days_in_month(year, month)) return std::nullopt;

  std::size_t pos = 10;
  int hour = 0, minute = 0, second = 0;
  if (pos < s.size() && (s[pos] == 'T' || s[pos] == ' ')) {
    if (!read_digits(s, pos + 1, 2, hour) || pos + 3 >= s.size() || s[pos + 3] != ':' ||
        !read_digits(s, pos + 4, 2, minute)) {
      return std::nullopt;
    }
    pos += 6;
    if (pos < s.size() && s[pos] == ':') {
      if (!read_digits(s, pos + 1, 2, second)) return std::nullopt;
      pos += 3;
      if (pos < s.size() && s[pos] == '.') {
        ++pos;
        while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') ++pos;
      }
    }
  }
  if (hour > 23 || minute > 59 || second > 59) return std::nullopt;

  Timestamp offset = 0;
  if (pos < s.size()) {
    if (s[pos] == 'Z' && pos + 1 == s.size()) {
      // UTC
    } else if ((s[pos] == '+' || s[pos] == '-') && pos + 6 == s.size() && s[pos + 3] == ':') {
      int oh = 0, om = 0;
      if (!read_digits(s, pos + 1, 2, oh) || !read_digits(s, pos + 4, 2, om)) return std::nullopt;
      offset = (s[pos] == '+' ? 1 : -1) * (Timestamp{oh} * 3600 + Timestamp{om} * 60);
    } else {
      return std::nullopt;
    }
  }

  const std::int64_t days =
      days_from_civil(year, static_cast<unsigned>(month), static_cast<unsigned>(day));
  return days * kSecondsPerDay + hour * 3600 + minute * 60 + second - offset;
}

std::string format_iso8601(Timestamp t) {
  std::int64_t days = t / kSecondsPerDay;
  std::int64_t rem = t % kSecondsPerDay;
  if (rem < 0) {
    rem += kSecondsPerDay;
    --days;
  }
  const CivilDate date = civil_from_days(days);
  char buf[32];
  std::snprintf(buf, sizeof buf, "%04lld-%02u-%02uT%02d:%02d:%02dZ",
                static_cast<long long>(date.year), date.month, date.day,
                static_cast<int>(rem / 3600), static_cast<int>(rem % 3600 / 60),
                static_cast<int>(rem % 60));
  return buf;
}

}  // namespace picks
