#include <cmath>

#include "check.hpp"
#include "picks/metrics.hpp"

using namespace picks;

TEST_CASE(scorecard_perfect_prediction) {
  Scorecard card;
  card.add({1.0, 0.0, 0.0}, Outcome::Home);
  CHECK_EQ(card.games(), std::size_t{1});
  CHECK_EQ(card.accuracy(), 1.0);
  CHECK_NEAR(card.brier(), 0.0, 1e-12);
  CHECK_NEAR(card.log_loss(), 0.0, 1e-12);
}

TEST_CASE(scorecard_uniform_prediction) {
  Scorecard card;
  card.add({1.0 / 3, 1.0 / 3, 1.0 / 3}, Outcome::Draw);
  CHECK_NEAR(card.brier(), 6.0 / 9.0, 1e-12);
  CHECK_NEAR(card.log_loss(), std::log(3.0), 1e-12);
  CHECK_EQ(card.accuracy(), 0.0);  // ties pick the home side
}

TEST_CASE(scorecard_averages_and_clamps) {
  Scorecard card;
  card.add({0.5, 0.0, 0.5}, Outcome::Away);
  card.add({0.0, 0.0, 1.0}, Outcome::Home);  // certain and wrong: log loss is clamped, not infinite
  CHECK_EQ(card.games(), std::size_t{2});
  CHECK_NEAR(card.brier(), (0.5 + 2.0) / 2, 1e-12);
  CHECK(std::isfinite(card.log_loss()));
  CHECK_EQ(Scorecard{}.accuracy(), 0.0);
}

TEST_CASE(probs_pick) {
  CHECK((Probs{0.4, 0.2, 0.4}.pick() == Outcome::Home));
  CHECK((Probs{0.3, 0.3, 0.4}.pick() == Outcome::Away));
  CHECK((Probs{0.3, 0.4, 0.3}.pick() == Outcome::Draw));
}
