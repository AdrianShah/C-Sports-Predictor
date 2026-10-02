#pragma once

#include <cstddef>

#include "picks/game.hpp"

namespace picks {

/// Accumulates accuracy, Brier score and log loss over a set of predictions.
/// Brier here is the multi-class form: sum over outcomes of (p - actual)^2,
/// so 0 is perfect and a coin flip on a two-way game scores 0.5.
class Scorecard {
 public:
  void add(const Probs& p, Outcome actual) noexcept;

  std::size_t games() const noexcept { return games_; }
  double accuracy() const noexcept;
  double brier() const noexcept;
  double log_loss() const noexcept;

 private:
  std::size_t games_ = 0;
  std::size_t correct_ = 0;
  double brier_sum_ = 0.0;
  double log_loss_sum_ = 0.0;
};

}  // namespace picks
