#pragma once

#include <filesystem>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "picks/csv.hpp"
#include "picks/game.hpp"

namespace picks {

struct HistoryError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

/// Converts a history CSV (columns: date, home, away, home_score, away_score and
/// optionally season, league, neutral, espn_id) into games. `source` names the
/// file in error messages.
std::vector<Game> games_from_csv(const CsvTable& table, std::string_view source);

/// Loads every root/<sport>/*.csv, sorted by kickoff, with duplicate IDs dropped.
std::vector<Game> load_history(const std::filesystem::path& root, Sport sport);

}  // namespace picks
