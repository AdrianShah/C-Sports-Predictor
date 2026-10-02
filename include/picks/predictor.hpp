#pragma once

#include <limits>
#include <string_view>

#include "picks/game.hpp"

namespace picks {

/// Anything that turns past games into outcome probabilities: the rating models
/// and the baselines they're measured against.
class Predictor {
 public:
  Predictor() = default;
  Predictor(const Predictor&) = delete;
  Predictor& operator=(const Predictor&) = delete;
  virtual ~Predictor() = default;

  virtual std::string_view name() const noexcept = 0;

  /// Probabilities for `game`, using only the games observed so far.
  virtual Probs predict(const Game& game) const = 0;

  /// Learns from a finished game. Games must arrive in chronological order.
  virtual void observe(const Game& game) = 0;

  /// False for baselines that only make a pick; their Brier score isn't meaningful.
  virtual bool probabilistic() const noexcept { return true; }

  /// Finished games seen for `team`. Predictors that don't track teams report "plenty".
  virtual int games_played(std::string_view /*team*/) const {
    return std::numeric_limits<int>::max();
  }
};

}  // namespace picks
