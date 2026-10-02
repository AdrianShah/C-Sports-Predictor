#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "picks/predictor.hpp"
#include "picks/strings.hpp"

namespace picks {

/// How the winning margin scales a rating update.
enum class MarginRule : std::uint8_t {
  None,         ///< every win counts the same
  Basketball,   ///< FiveThirtyEight NBA: (mov + 3)^0.8 / (7.5 + 0.006 * elo_diff)
  Logarithmic,  ///< FiveThirtyEight NFL/NHL style: ln(mov + 1) * 2.2 / (0.001 * elo_diff + 2.2)
};

struct EloParams {
  double initial = 1500.0;
  double k = 20.0;
  double home_advantage = 100.0;
  double carry_over = 0.75;  ///< share of (rating - initial) kept after an offseason
  int offseason_days = 60;   ///< a longer break than this counts as a new season
  double draw_rate = 0.0;    ///< draw probability of an even game (0 for sports without draws)
  MarginRule margin = MarginRule::Logarithmic;
};

/// Expected score of a side rated `diff` points higher.
double elo_expected(double diff) noexcept;

/// Splits an expected score into home/draw/away probabilities. The draw share is
/// largest for even games and shrinks linearly as one side gets stronger.
Probs split_expected(double expected, double draw_rate) noexcept;

/// Update multiplier for a winning `margin`, where `winner_diff` is the winner's
/// pre-game rating edge (it damps runaway updates for favourites).
double margin_multiplier(MarginRule rule, int margin, double winner_diff) noexcept;

struct TeamRating {
  std::string team;
  double rating = 0.0;
  int games = 0;
};

class EloModel final : public Predictor {
 public:
  explicit EloModel(EloParams params) : params_(params) {}

  std::string_view name() const noexcept override { return "Elo"; }
  Probs predict(const Game& game) const override;
  void observe(const Game& game) override;
  int games_played(std::string_view team) const override;

  /// Rating as of `at`, including any offseason regression still to be applied.
  double rating(std::string_view team, Timestamp at) const;
  /// All teams, strongest first.
  std::vector<TeamRating> table(Timestamp at) const;
  const EloParams& params() const noexcept { return params_; }

 private:
  struct Team {
    double rating = 0.0;
    int games = 0;
    Timestamp last_played = 0;
  };

  const Team* find(std::string_view team) const;
  double effective(const Team* team, Timestamp at) const noexcept;

  EloParams params_;
  std::unordered_map<std::string, Team, StringHash, std::equal_to<>> teams_;
};

}  // namespace picks
