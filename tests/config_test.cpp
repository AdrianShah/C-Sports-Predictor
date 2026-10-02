#include <stdexcept>

#include "check.hpp"
#include "picks/config.hpp"

using namespace picks;

TEST_CASE(config_applies_settings) {
  SportConfig c = default_config(Sport::Soccer);
  apply_setting(c, "elo.k=25");
  apply_setting(c, "goals.rho=-0.12");
  apply_setting(c, "goals.offseason_days=90");
  CHECK_EQ(c.elo.k, 25.0);
  CHECK_EQ(c.goals.rho, -0.12);
  CHECK_EQ(c.goals.offseason_days, 90);
}

TEST_CASE(config_rejects_bad_settings) {
  SportConfig c = default_config(Sport::Nba);
  CHECK_THROWS(apply_setting(c, "elo.k"), std::invalid_argument);
  CHECK_THROWS(apply_setting(c, "elo.k=abc"), std::invalid_argument);
  CHECK_THROWS(apply_setting(c, "elo.k="), std::invalid_argument);
  CHECK_THROWS(apply_setting(c, "elo.bogus=1"), std::invalid_argument);
  CHECK_THROWS(apply_setting(c, "goals.max_goals=2.5"), std::invalid_argument);
}

TEST_CASE(config_model_of_record) {
  CHECK_EQ(std::string(make_predictors(default_config(Sport::Soccer)).front()->name()),
           std::string("Dixon-Coles"));
  CHECK_EQ(std::string(make_predictors(default_config(Sport::Nba)).front()->name()), std::string("Elo"));
  CHECK_EQ(default_config(Sport::Nhl).elo.draw_rate, 0.0);
  CHECK(default_config(Sport::Soccer).elo.draw_rate > 0.0);
}
