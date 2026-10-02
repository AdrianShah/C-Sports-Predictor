#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "picks/game.hpp"
#include "picks/json.hpp"
#include "picks/predictor.hpp"

namespace picks {

/// An event from the profile repo's data/fixtures.json ("a" is the home side).
struct Fixture {
  std::string id;
  Sport sport = Sport::Nba;
  std::string competition;
  std::string home;
  std::string away;
  Timestamp start = 0;
  std::string status;
  std::string home_logo;  ///< optional; passed through to the output for display
  std::string away_logo;
};

/// Events of supported sports; others (and events with bad timestamps) are skipped.
std::vector<Fixture> read_fixtures(const Json& doc);

/// One entry of data/model_picks.json. `pick` uses the same convention as the
/// app's picks.json: a team name, or "draw".
struct ModelPick {
  std::string event_id;
  std::string sport;
  std::string competition;
  std::string home;
  std::string away;
  Timestamp scheduled_at = 0;
  Timestamp locked_at = 0;
  std::string pick;
  Probs probs;
  std::string home_logo;
  std::string away_logo;
};

std::vector<ModelPick> read_model_picks(const Json& doc);
/// Serializes with one pick per line, so the bot's commits diff cleanly.
std::string write_model_picks(const std::vector<ModelPick>& picks, Timestamp generated_at);

struct PredictOptions {
  Timestamp now = 0;
  int min_games = 10;  ///< skip games where the model has seen too little of a team
  std::vector<std::string> exclude_competitions;  ///< see competition_excluded
};

/// Case-insensitive: a pattern matches as a substring, or as the whole name if it
/// starts with '=' ("=spanish liga f" must not catch "Liga FPD").
bool competition_excluded(std::string_view competition, const std::vector<std::string>& patterns);

struct PredictStats {
  std::size_t upcoming = 0;
  std::size_t picked = 0;
  std::size_t excluded = 0;
  std::size_t unknown_teams = 0;
};

/// Picks for fixtures of `sport` that haven't started yet, from a trained model.
std::vector<ModelPick> predict_fixtures(const Predictor& model, Sport sport,
                                        const std::vector<Fixture>& fixtures,
                                        const PredictOptions& options, PredictStats& stats);

/// Combines the previous file with fresh picks:
/// - picks for games that have started are frozen, whatever the new run says;
/// - an unchanged pick keeps its original lockedAt;
/// - earlier picks the new run didn't produce (e.g. a fixture dropped out) are kept.
std::vector<ModelPick> merge_picks(std::vector<ModelPick> previous, std::vector<ModelPick> fresh,
                                   Timestamp now);

}  // namespace picks
