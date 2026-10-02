#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include "picks/elo.hpp"
#include "picks/game.hpp"
#include "picks/goals.hpp"
#include "picks/predictor.hpp"

namespace picks {

struct SportConfig {
  Sport sport = Sport::Nba;
  EloParams elo;
  GoalsParams goals;
};

/// Tuned defaults for each sport.
SportConfig default_config(Sport sport);

/// Applies an override such as "elo.k=24" or "goals.rho=-0.1".
/// Throws std::invalid_argument for unknown keys or non-numeric values.
void apply_setting(SportConfig& config, std::string_view assignment);

/// The model of record first (Dixon–Coles for soccer, Elo otherwise), then the
/// models and baselines it is compared against.
std::vector<std::unique_ptr<Predictor>> make_predictors(const SportConfig& config);

}  // namespace picks
