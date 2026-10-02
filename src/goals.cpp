#include "picks/goals.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace picks {
namespace {

std::vector<double> poisson_pmf(double rate, int max_goals) {
  std::vector<double> p(static_cast<std::size_t>(max_goals) + 1);
  p[0] = std::exp(-rate);
  for (std::size_t k = 1; k < p.size(); ++k) p[k] = p[k - 1] * rate / static_cast<double>(k);
  return p;
}

}  // namespace

Probs dixon_coles(double home_rate, double away_rate, double rho, int max_goals) {
  const auto ph = poisson_pmf(home_rate, max_goals);
  const auto pa = poisson_pmf(away_rate, max_goals);
  const auto tau = [&](std::size_t i, std::size_t j) {
    double t = 1.0;
    if (i == 0 && j == 0) t = 1.0 - home_rate * away_rate * rho;
    else if (i == 0 && j == 1) t = 1.0 + home_rate * rho;
    else if (i == 1 && j == 0) t = 1.0 + away_rate * rho;
    else if (i == 1 && j == 1) t = 1.0 - rho;
    return std::max(t, 0.0);
  };

  Probs p;
  for (std::size_t i = 0; i < ph.size(); ++i) {
    for (std::size_t j = 0; j < pa.size(); ++j) {
      const double cell = ph[i] * pa[j] * tau(i, j);
      if (i > j) p.home += cell;
      else if (i == j) p.draw += cell;
      else p.away += cell;
    }
  }
  const double total = p.home + p.draw + p.away;
  if (total <= 0.0) return {1.0 / 3, 1.0 / 3, 1.0 / 3};
  return {p.home / total, p.draw / total, p.away / total};
}

const GoalsModel::Team* GoalsModel::find(std::string_view team) const {
  const auto it = teams_.find(team);
  return it == teams_.end() ? nullptr : &it->second;
}

GoalsModel::Strength GoalsModel::effective(const Team* team, Timestamp at) const noexcept {
  if (!team) return {};
  const Timestamp gap = at - team->last_played;
  if (team->games > 0 && gap > Timestamp{params_.offseason_days} * kSecondsPerDay) {
    return {team->attack * params_.carry_over, team->defence * params_.carry_over};
  }
  return {team->attack, team->defence};
}

GoalRates GoalsModel::rates(Strength home, Strength away, bool neutral) const noexcept {
  const double edge = neutral ? 0.0 : home_edge_;
  return {std::exp(std::clamp(level_ + edge + home.attack - away.defence, -4.0, 2.5)),
          std::exp(std::clamp(level_ + away.attack - home.defence, -4.0, 2.5))};
}

double GoalsModel::team_rate(int games) const noexcept {
  return std::max(params_.learning_rate, 1.0 / (games + params_.prior_games));
}

GoalRates GoalsModel::expected_goals(const Game& game) const {
  return rates(effective(find(game.home), game.start), effective(find(game.away), game.start),
               game.neutral);
}

Probs GoalsModel::predict(const Game& game) const {
  const GoalRates r = expected_goals(game);
  return dixon_coles(r.home, r.away, params_.rho, params_.max_goals);
}

void GoalsModel::observe(const Game& game) {
  if (game.home == game.away) return;
  Team& home = teams_.try_emplace(game.home).first->second;
  Team& away = teams_.try_emplace(game.away).first->second;
  const Strength h = effective(&home, game.start);
  const Strength a = effective(&away, game.start);
  const GoalRates expected = rates(h, a, game.neutral);

  // Gradient of the Poisson log-likelihood: observed minus expected goals.
  // Scores are capped so one freak result can't swing a rating.
  const double home_error = std::min(game.home_score, 8) - expected.home;
  const double away_error = std::min(game.away_score, 8) - expected.away;
  const double home_step = team_rate(home.games);
  const double away_step = team_rate(away.games);

  home.attack = h.attack + home_step * home_error;
  home.defence = h.defence - home_step * away_error;
  away.attack = a.attack + away_step * away_error;
  away.defence = a.defence - away_step * home_error;

  level_ += params_.global_rate * (home_error + away_error);
  if (!game.neutral) home_edge_ += params_.global_rate * (home_error - away_error);

  for (Team* t : {&home, &away}) {
    ++t->games;
    t->last_played = game.start;
  }
}

int GoalsModel::games_played(std::string_view team) const {
  const Team* t = find(team);
  return t ? t->games : 0;
}

}  // namespace picks
