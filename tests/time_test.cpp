#include "check.hpp"
#include "picks/time.hpp"

using namespace picks;

static_assert(days_from_civil(1970, 1, 1) == 0);
static_assert(days_from_civil(2000, 3, 1) == 11'017);
static_assert(civil_from_days(-1).year == 1969);

TEST_CASE(civil_dates_round_trip) {
  for (std::int64_t day = -800'000; day <= 800'000; day += 97) {
    const CivilDate d = civil_from_days(day);
    CHECK_EQ(days_from_civil(d.year, d.month, d.day), day);
  }
}

TEST_CASE(parses_iso8601_variants) {
  const Timestamp day = days_from_civil(2025, 2, 22) * kSecondsPerDay;
  CHECK_EQ(parse_iso8601("2025-02-22T12:30Z").value(), day + 12 * 3600 + 30 * 60);
  CHECK_EQ(parse_iso8601("2025-02-22T12:30:15.250Z").value(), day + 12 * 3600 + 30 * 60 + 15);
  CHECK_EQ(parse_iso8601("2025-02-22").value(), day);
  CHECK_EQ(parse_iso8601("2025-02-22T14:30:00+02:00").value(), day + 12 * 3600 + 30 * 60);
  CHECK_EQ(parse_iso8601("2025-02-22T07:30:00-05:00").value(), day + 12 * 3600 + 30 * 60);
}

TEST_CASE(rejects_invalid_times) {
  for (const char* bad : {"", "garbage", "2026-02-30", "2026-13-01", "2026-07-04T25:00Z",
                          "2026-07-04T16:00Zjunk", "2026-07-04T16", "20260704"}) {
    if (parse_iso8601(bad)) ::check::fail(__FILE__, __LINE__, std::string("accepted '") + bad + "'");
  }
  CHECK(parse_iso8601("2024-02-29").has_value());
  CHECK(!parse_iso8601("2025-02-29").has_value());
}

TEST_CASE(formats_iso8601) {
  CHECK_EQ(format_iso8601(0), std::string("1970-01-01T00:00:00Z"));
  CHECK_EQ(format_iso8601(parse_iso8601("2026-10-03T23:00:00.000Z").value()),
           std::string("2026-10-03T23:00:00Z"));
  CHECK_EQ(format_iso8601(-1), std::string("1969-12-31T23:59:59Z"));
}
