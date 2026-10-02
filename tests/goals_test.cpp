#include "check.hpp"
#include "picks/goals.hpp"

using namespace picks;

namespace {

Game game(Timestamp day, std::string home, std::string away, int hs, int as, bool neutral = false) {
  return Game{.start = day * kSecondsPerDay, .home = std::move(home), .away = std::move(away),
              .home_score = hs, .away_score = as, .neutral = neutral};
}

}  // namespace

TEST_CASE(dixon_coles_matches_independent_poisson) {
  // With rho = 0 the draw probability for two equal rates of 1.5 is
  // sum_k P(k)^2 = e^-3 * I0(3) ≈ 0.24300.
  const Probs p = dixon_coles(1.5, 1.5, 0.0, 12);
  CHECK_NEAR(p.home + p.draw + p.away, 1.0, 1e-12);
  CHECK_NEAR(p.home, p.away, 1e-12);
  CHECK_NEAR(p.draw, 0.2430, 5e-4);
}

TEST_CASE(dixon_coles_negative_rho_adds_draws) {
  CHECK(dixon_coles(1.2, 1.2, -0.1, 10).draw > dixon_coles(1.2, 1.2, 0.0, 10).draw);
  CHECK(dixon_coles(2.0, 0.8, 0.0, 10).home > 0.6);
}

TEST_CASE(goals_model_learns_strengths) {
  GoalsModel model(GoalsParams{});
  for (int d = 0; d < 60; ++d) {
    model.observe(d % 2 ? game(d, "City", "Town", 3, 0) : game(d, "Town", "City", 0, 3));
    model.observe(game(d, "Mid", "Mid B", 1, 1));
  }
  const Game next = game(61, "Town", "City", 0, 0);
  const GoalRates rates = model.expected_goals(next);
  CHECK(rates.away > rates.home);
  CHECK(model.predict(next).away > 0.6);
  CHECK_EQ(model.games_played("City"), 60);
  CHECK_EQ(model.games_played("Nobody"), 0);
}

TEST_CASE(goals_model_home_edge_and_neutral) {
  const GoalsModel model(GoalsParams{});
  const GoalRates home = model.expected_goals(game(0, "A", "B", 0, 0));
  const GoalRates neutral = model.expected_goals(game(0, "A", "B", 0, 0, true));
  CHECK(home.home > home.away);
  CHECK_NEAR(neutral.home, neutral.away, 1e-12);
}

TEST_CASE(goals_model_regresses_after_offseason) {
  GoalsModel model({.carry_over = 0.5, .offseason_days = 75});
  for (int d = 0; d < 30; ++d) model.observe(game(d, "A", "B", 4, 0, true));
  const double gap_now = model.expected_goals(game(31, "A", "B", 0, 0, true)).home -
                         model.expected_goals(game(31, "A", "B", 0, 0, true)).away;
  const double gap_later = model.expected_goals(game(200, "A", "B", 0, 0, true)).home -
                           model.expected_goals(game(200, "A", "B", 0, 0, true)).away;
  CHECK(gap_now > 0);
  CHECK(gap_later < gap_now);
  CHECK(gap_later > 0);
}
