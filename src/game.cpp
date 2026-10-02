#include "picks/game.hpp"

namespace picks {

std::optional<Sport> parse_sport(std::string_view name) noexcept {
  if (name == "soccer") return Sport::Soccer;
  if (name == "nba") return Sport::Nba;
  if (name == "nhl") return Sport::Nhl;
  return std::nullopt;
}

std::string_view sport_name(Sport sport) noexcept {
  switch (sport) {
    case Sport::Soccer: return "soccer";
    case Sport::Nba: return "nba";
    case Sport::Nhl: return "nhl";
  }
  return "unknown";
}

}  // namespace picks
