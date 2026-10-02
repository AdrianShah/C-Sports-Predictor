#pragma once

#include <memory>
#include <string>
#include <vector>

#include "picks/game.hpp"
#include "picks/json.hpp"
#include "picks/metrics.hpp"
#include "picks/predictor.hpp"

namespace picks {

struct BacktestOptions {
  int warmup_days = 365;  ///< games in the first year only train the models
  int min_games = 10;     ///< both teams need this many games (per the first model) to be scored
};

struct BacktestRow {
  std::string name;
  bool probabilistic = true;
  Scorecard card;
};

struct BacktestReport {
  Sport sport = Sport::Nba;
  std::size_t games = 0;   ///< all games replayed
  std::size_t scored = 0;  ///< games every predictor was scored on
  Timestamp first = 0;
  Timestamp last = 0;
  Timestamp scored_from = 0;
  std::vector<BacktestRow> rows;  ///< one per predictor, model of record first
};

/// Walk-forward evaluation: each game is predicted by every predictor using only
/// earlier games, then all predictors learn from it. `games` must be sorted.
BacktestReport run_backtest(Sport sport, const std::vector<Game>& games,
                            const std::vector<std::unique_ptr<Predictor>>& predictors,
                            const BacktestOptions& options);

std::string format_markdown(const BacktestReport& report);
Json to_json(const BacktestReport& report);

}  // namespace picks
