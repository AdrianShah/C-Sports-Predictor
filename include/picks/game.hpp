#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "picks/time.hpp"

namespace picks {

enum class Sport : std::uint8_t { Soccer, Nba, Nhl };

inline constexpr std::array kAllSports{Sport::Soccer, Sport::Nba, Sport::Nhl};

/// "soccer", "nba", "nhl" (the `sport` values used in fixtures.json).
std::optional<Sport> parse_sport(std::string_view name) noexcept;
std::string_view sport_name(Sport sport) noexcept;

/// Only soccer can end level; NBA and NHL always produce a winner.
constexpr bool allows_draw(Sport sport) noexcept { return sport == Sport::Soccer; }

enum class Outcome : std::uint8_t { Home, Draw, Away };

/// A finished game (or, with zero scores, a fixture to predict).
struct Game {
  Timestamp start = 0;
  int season = 0;
  std::string league;
  std::string home;
  std::string away;
  int home_score = 0;
  int away_score = 0;
  bool neutral = false;
  std::string id;

  int margin() const noexcept { return home_score - away_score; }
  Outcome outcome() const noexcept {
    if (home_score > away_score) return Outcome::Home;
    if (home_score < away_score) return Outcome::Away;
    return Outcome::Draw;
  }
};

/// Outcome probabilities from the home side's point of view.
struct Probs {
  double home = 0.0;
  double draw = 0.0;
  double away = 0.0;

  double operator[](Outcome o) const noexcept {
    switch (o) {
      case Outcome::Home: return home;
      case Outcome::Draw: return draw;
      case Outcome::Away: return away;
    }
    return 0.0;
  }

  /// Most likely outcome; ties go to the home side, then the away side.
  Outcome pick() const noexcept {
    if (home >= draw && home >= away) return Outcome::Home;
    if (away >= draw) return Outcome::Away;
    return Outcome::Draw;
  }
};

}  // namespace picks
