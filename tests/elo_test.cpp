#include "check.hpp"
#include "picks/elo.hpp"

using namespace picks;

namespace {

Game game(Timestamp day, std::string home, std::string away, int hs, int as) {
  return Game{.start = day * kSecondsPerDay, .home = std::move(home), .away = std::move(away),
              .home_score = hs, .away_score = as};
}

}  // namespace

TEST_CASE(elo_expected_score) {
  CHECK_NEAR(elo_expected(0), 0.5, 1e-12);
  CHECK_NEAR(elo_expected(400), 10.0 / 11.0, 1e-12);
  CHECK_NEAR(elo_expected(-200) + elo_expected(200), 1.0, 1e-12);
}

TEST_CASE(elo_split_keeps_expected_score) {
  const Probs even = split_expected(0.5, 0.28);
  CHECK_NEAR(even.draw, 0.28, 1e-12);
  CHECK_NEAR(even.home, 0.36, 1e-12);
  for (double e = 0.0; e <= 1.0; e += 0.05) {
    const Probs p = split_expected(e, 0.28);
    CHECK_NEAR(p.home + p.draw + p.away, 1.0, 1e-12);
    CHECK_NEAR(p.home + p.draw / 2, e, 1e-12);
    CHECK(p.home >= 0 && p.away >= 0);
  }
  CHECK_EQ(split_expected(0.7, 0.0).draw, 0.0);
}

TEST_CASE(elo_margin_multiplier) {
  CHECK_EQ(margin_multiplier(MarginRule::Logarithmic, 0, 0), 1.0);
  CHECK_EQ(margin_multiplier(MarginRule::None, 20, 0), 1.0);
  CHECK(margin_multiplier(MarginRule::Logarithmic, 4, 0) > margin_multiplier(MarginRule::Logarithmic, 1, 0));
  CHECK(margin_multiplier(MarginRule::Basketball, 20, 0) > margin_multiplier(MarginRule::Basketball, 2, 0));
  // A favourite that wins big gains less than an underdog winning by the same margin.
  CHECK(margin_multiplier(MarginRule::Basketball, 15, 200) < margin_multiplier(MarginRule::Basketball, 15, -200));
}

TEST_CASE(elo_update_is_zero_sum) {
  EloModel elo({.k = 20, .home_advantage = 0, .margin = MarginRule::None});
  elo.observe(game(0, "A", "B", 101, 100));
  CHECK_NEAR(elo.rating("A", 0), 1510.0, 1e-9);
  CHECK_NEAR(elo.rating("B", 0), 1490.0, 1e-9);
  elo.observe(game(1, "B", "C", 90, 95));
  CHECK_NEAR(elo.rating("A", 1) + elo.rating("B", 1) + elo.rating("C", 1), 4500.0, 1e-9);
  CHECK_EQ(elo.games_played("B"), 2);
  CHECK_EQ(elo.games_played("nobody"), 0);
}

TEST_CASE(elo_home_advantage_and_neutral_sites) {
  EloModel elo({.home_advantage = 100});
  Game g = game(0, "A", "B", 0, 0);
  CHECK(elo.predict(g).home > 0.6);
  g.neutral = true;
  CHECK_NEAR(elo.predict(g).home, 0.5, 1e-12);
}

TEST_CASE(elo_regresses_after_offseason) {
  EloModel elo({.k = 20, .home_advantage = 0, .carry_over = 0.75, .offseason_days = 60,
                .margin = MarginRule::None});
  for (int d = 0; d < 30; ++d) elo.observe(game(d, "A", "B", 2, 1));
  const double in_season = elo.rating("A", 30 * kSecondsPerDay);
  const double next_season = elo.rating("A", 200 * kSecondsPerDay);
  CHECK(in_season > 1600);
  CHECK_NEAR(next_season, 1500 + 0.75 * (in_season - 1500), 1e-9);
}

TEST_CASE(elo_learns_the_stronger_team) {
  EloModel elo({.k = 20, .home_advantage = 50, .margin = MarginRule::Logarithmic});
  for (int d = 0; d < 40; ++d) {
    elo.observe(d % 2 ? game(d, "Strong", "Weak", 110, 95) : game(d, "Weak", "Strong", 95, 110));
  }
  CHECK(elo.predict(game(41, "Weak", "Strong", 0, 0)).away > 0.75);
  const auto table = elo.table(41 * kSecondsPerDay);
  CHECK_EQ(table.front().team, std::string("Strong"));
}
