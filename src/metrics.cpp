#include "picks/metrics.hpp"

#include <algorithm>
#include <cmath>

namespace picks {

void Scorecard::add(const Probs& p, Outcome actual) noexcept {
  ++games_;
  if (p.pick() == actual) ++correct_;
  for (const Outcome o : {Outcome::Home, Outcome::Draw, Outcome::Away}) {
    const double target = o == actual ? 1.0 : 0.0;
    brier_sum_ += (p[o] - target) * (p[o] - target);
  }
  log_loss_sum_ -= std::log(std::max(p[actual], 1e-15));
}

double Scorecard::accuracy() const noexcept {
  return games_ ? static_cast<double>(correct_) / static_cast<double>(games_) : 0.0;
}

double Scorecard::brier() const noexcept {
  return games_ ? brier_sum_ / static_cast<double>(games_) : 0.0;
}

double Scorecard::log_loss() const noexcept {
  return games_ ? log_loss_sum_ / static_cast<double>(games_) : 0.0;
}

}  // namespace picks
