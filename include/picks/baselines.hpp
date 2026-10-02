#pragma once

#include <array>
#include <functional>
#include <string>
#include <unordered_map>

#include "picks/predictor.hpp"
#include "picks/strings.hpp"

namespace picks {

/// Always picks the home side.
class AlwaysHome final : public Predictor {
 public:
  std::string_view name() const noexcept override { return "Always home"; }
  Probs predict(const Game&) const override { return {1.0, 0.0, 0.0}; }
  void observe(const Game&) override {}
  bool probabilistic() const noexcept override { return false; }
};

/// Predicts the historical home/draw/away frequencies for every game. The Brier
/// score to beat: a model that knows nothing about the teams.
class BaseRate final : public Predictor {
 public:
  explicit BaseRate(bool draws) : counts_{1.0, draws ? 1.0 : 0.0, 1.0} {}
  std::string_view name() const noexcept override { return "Base rate"; }
  Probs predict(const Game&) const override;
  void observe(const Game& game) override;

 private:
  std::array<double, 3> counts_;  // home, draw, away (with a +1 prior)
};

/// Picks the side with more points per game this season (win 2, draw 1); ties go home.
class BetterRecord final : public Predictor {
 public:
  std::string_view name() const noexcept override { return "Better record"; }
  Probs predict(const Game& game) const override;
  void observe(const Game& game) override;
  bool probabilistic() const noexcept override { return false; }

 private:
  struct Record {
    int season = 0;
    int points = 0;
    int games = 0;
  };
  double points_per_game(std::string_view team, int season) const;

  std::unordered_map<std::string, Record, StringHash, std::equal_to<>> records_;
};

}  // namespace picks
