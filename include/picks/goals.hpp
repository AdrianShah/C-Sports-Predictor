#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include "picks/predictor.hpp"
#include "picks/strings.hpp"

namespace picks {

struct GoalsParams {
  double learning_rate = 0.03;  ///< step size for an established team's attack/defence
  double prior_games = 10.0;    ///< new teams learn at 1 / (games + prior_games) until that's smaller
  double global_rate = 0.001;   ///< step size for the overall goal level and home edge
  double rho = -0.08;           ///< Dixon–Coles low-score dependence (negative = more 0-0 and 1-1)
  double carry_over = 0.8;      ///< share of attack/defence kept after an offseason
  int offseason_days = 75;
  int max_goals = 10;           ///< score grid size when summing outcome probabilities
};

struct GoalRates {
  double home = 0.0;
  double away = 0.0;
};

/// P(home win / draw / away win) when goals are Poisson with the given rates,
/// with the Dixon–Coles correction for 0-0, 1-0, 0-1 and 1-1.
Probs dixon_coles(double home_rate, double away_rate, double rho, int max_goals);

/// Soccer goals model. Each team has an attack and a defence strength (log scale):
///   home goals ~ Poisson(exp(level + home_edge + attack[home] - defence[away]))
///   away goals ~ Poisson(exp(level + attack[away] - defence[home]))
/// Strengths are fitted online, one game at a time, by a gradient step on the
/// Poisson log-likelihood, so the model can be replayed walk-forward.
class GoalsModel final : public Predictor {
 public:
  explicit GoalsModel(GoalsParams params) : params_(params) {}

  std::string_view name() const noexcept override { return "Dixon-Coles"; }
  Probs predict(const Game& game) const override;
  void observe(const Game& game) override;
  int games_played(std::string_view team) const override;

  GoalRates expected_goals(const Game& game) const;

 private:
  struct Team {
    double attack = 0.0;
    double defence = 0.0;
    int games = 0;
    Timestamp last_played = 0;
  };
  struct Strength {
    double attack = 0.0;
    double defence = 0.0;
  };

  const Team* find(std::string_view team) const;
  Strength effective(const Team* team, Timestamp at) const noexcept;
  GoalRates rates(Strength home, Strength away, bool neutral) const noexcept;
  double team_rate(int games) const noexcept;

  GoalsParams params_;
  double level_ = 0.3;       ///< log goals per team at a neutral ground (e^0.3 ≈ 1.35)
  double home_edge_ = 0.25;  ///< log home-goal boost
  std::unordered_map<std::string, Team, StringHash, std::equal_to<>> teams_;
};

}  // namespace picks
