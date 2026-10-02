#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace picks {

/// Seconds since the Unix epoch, UTC.
using Timestamp = std::int64_t;

inline constexpr Timestamp kSecondsPerDay = 86'400;

struct CivilDate {
  std::int64_t year;
  unsigned month;  // 1-12
  unsigned day;    // 1-31
};

/// Days since 1970-01-01 for a proleptic Gregorian date (Howard Hinnant's algorithm).
constexpr std::int64_t days_from_civil(std::int64_t y, unsigned m, unsigned d) noexcept {
  y -= m <= 2 ? 1 : 0;
  const std::int64_t era = (y >= 0 ? y : y - 399) / 400;
  const auto yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146'097 + static_cast<std::int64_t>(doe) - 719'468;
}

/// Inverse of days_from_civil.
constexpr CivilDate civil_from_days(std::int64_t z) noexcept {
  z += 719'468;
  const std::int64_t era = (z >= 0 ? z : z - 146'096) / 146'097;
  const auto doe = static_cast<unsigned>(z - era * 146'097);
  const unsigned yoe = (doe - doe / 1'460 + doe / 36'524 - doe / 146'096) / 365;
  const std::int64_t y = static_cast<std::int64_t>(yoe) + era * 400;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  const unsigned d = doy - (153 * mp + 2) / 5 + 1;
  const unsigned m = mp < 10 ? mp + 3 : mp - 9;
  return {y + (m <= 2 ? 1 : 0), m, d};
}

/// Parses "2026-10-03", "2026-10-03T23:00Z", "2026-10-03T23:00:00.000Z" or a
/// "+02:00"-style offset. A missing zone means UTC.
std::optional<Timestamp> parse_iso8601(std::string_view text);

/// Formats as "2026-10-03T23:00:00Z".
std::string format_iso8601(Timestamp t);

}  // namespace picks
