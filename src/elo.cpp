#include "picks/elo.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace picks {

double elo_expected(double diff) noexcept { return 1.0 / (1.0 + std::pow(10.0, -diff / 400.0)); }

Probs split_expected(double expected, double draw_rate) noexcept {
  const double e = std::clamp(expected, 0.0, 1.0);
  const double draw = std::clamp(draw_rate, 0.0, 1.0) * (1.0 - std::abs(2.0 * e - 1.0));
  // expected = P(home) + P(draw) / 2 holds by construction.
  return {e - draw / 2.0, draw, 1.0 - e - draw / 2.0};
}

double margin_multiplier(MarginRule rule, int margin, double winner_diff) noexcept {
  const int m = std::abs(margin);
  if (m == 0) return 1.0;
  switch (rule) {
    case MarginRule::None:
      return 1.0;
    case MarginRule::Basketball:
      return std::pow(m + 3.0, 0.8) / std::max(7.5 + 0.006 * winner_diff, 1.0);
    case MarginRule::Logarithmic:
      return std::log(m + 1.0) * 2.2 / std::max(0.001 * winner_diff + 2.2, 0.5);
  }
  return 1.0;
}

const EloModel::Team* EloModel::find(std::string_view team) const {
  const auto it = teams_.find(team);
  return it == teams_.end() ? nullptr : &it->second;
}

double EloModel::effective(const Team* team, Timestamp at) const noexcept {
  if (!team) return params_.initial;
  const Timestamp gap = at - team->last_played;
  if (team->games > 0 && gap > Timestamp{params_.offseason_days} * kSecondsPerDay) {
    return params_.initial + params_.carry_over * (team->rating - params_.initial);
  }
  return team->rating;
}

Probs EloModel::predict(const Game& game) const {
  const double diff = effective(find(game.home), game.start) - effective(find(game.away), game.start) +
                      (game.neutral ? 0.0 : params_.home_advantage);
  return split_expected(elo_expected(diff), params_.draw_rate);
}

void EloModel::observe(const Game& game) {
  if (game.home == game.away) return;
  Team& home = teams_.try_emplace(game.home, Team{params_.initial}).first->second;
  Team& away = teams_.try_emplace(game.away, Team{params_.initial}).first->second;
  home.rating = effective(&home, game.start);
  away.rating = effective(&away, game.start);

  const double diff = home.rating - away.rating + (game.neutral ? 0.0 : params_.home_advantage);
  const double expected = elo_expected(diff);
  double actual = 0.5;
  if (game.outcome() == Outcome::Home) actual = 1.0;
  if (game.outcome() == Outcome::Away) actual = 0.0;
  const double winner_diff = actual > 0.5 ? diff : -diff;
  const double delta =
      params_.k * margin_multiplier(params_.margin, game.margin(), winner_diff) * (actual - expected);

  home.rating += delta;
  away.rating -= delta;
  for (Team* t : {&home, &away}) {
    ++t->games;
    t->last_played = game.start;
  }
}

int EloModel::games_played(std::string_view team) const {
  const Team* t = find(team);
  return t ? t->games : 0;
}

double EloModel::rating(std::string_view team, Timestamp at) const { return effective(find(team), at); }

std::vector<TeamRating> EloModel::table(Timestamp at) const {
  std::vector<TeamRating> out;
  out.reserve(teams_.size());
  for (const auto& [name, team] : teams_) out.push_back({name, effective(&team, at), team.games});
  std::sort(out.begin(), out.end(), [](const TeamRating& a, const TeamRating& b) {
    return a.rating != b.rating ? a.rating > b.rating : a.team < b.team;
  });
  return out;
}

}  // namespace picks
