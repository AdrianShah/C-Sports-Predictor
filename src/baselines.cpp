#include "picks/baselines.hpp"

namespace picks {

Probs BaseRate::predict(const Game&) const {
  const double total = counts_[0] + counts_[1] + counts_[2];
  return {counts_[0] / total, counts_[1] / total, counts_[2] / total};
}

void BaseRate::observe(const Game& game) { counts_[static_cast<std::size_t>(game.outcome())] += 1.0; }

double BetterRecord::points_per_game(std::string_view team, int season) const {
  const auto it = records_.find(team);
  if (it == records_.end() || it->second.season != season || it->second.games == 0) return 0.0;
  return static_cast<double>(it->second.points) / it->second.games;
}

Probs BetterRecord::predict(const Game& game) const {
  const bool home = points_per_game(game.home, game.season) >= points_per_game(game.away, game.season);
  return home ? Probs{1.0, 0.0, 0.0} : Probs{0.0, 0.0, 1.0};
}

void BetterRecord::observe(const Game& game) {
  const auto add = [&](const std::string& team, int points) {
    Record& r = records_[team];
    if (r.season != game.season) r = Record{game.season, 0, 0};
    r.points += points;
    ++r.games;
  };
  const Outcome o = game.outcome();
  add(game.home, o == Outcome::Home ? 2 : o == Outcome::Draw ? 1 : 0);
  add(game.away, o == Outcome::Away ? 2 : o == Outcome::Draw ? 1 : 0);
}

}  // namespace picks
