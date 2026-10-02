#include "check.hpp"
#include "picks/backtest.hpp"
#include "picks/config.hpp"

using namespace picks;

namespace {

/// Two seasons of a four-team league where the ranking never changes.
std::vector<Game> ranked_league() {
  const std::vector<std::string> teams = {"Best", "Good", "Fair", "Poor"};
  std::vector<Game> games;
  Timestamp day = 0;
  for (int round = 0; round < 60; ++round) {
    for (std::size_t i = 0; i < teams.size(); ++i) {
      for (std::size_t j = 0; j < teams.size(); ++j) {
        if (i == j) continue;
        // The better team (lower index) wins by 5, home or away.
        const bool home_better = i < j;
        games.push_back({.start = day++ * kSecondsPerDay,
                         .season = round < 30 ? 2025 : 2026,
                         .home = teams[i],
                         .away = teams[j],
                         .home_score = home_better ? 105 : 100,
                         .away_score = home_better ? 100 : 105});
      }
    }
  }
  return games;
}

}  // namespace

TEST_CASE(backtest_scores_after_warmup) {
  const auto games = ranked_league();
  const auto predictors = make_predictors(default_config(Sport::Nba));
  // One game a day, 12 per round: 96 days of warmup is exactly 8 rounds.
  const auto report = run_backtest(Sport::Nba, games, predictors, {.warmup_days = 96, .min_games = 5});

  CHECK_EQ(report.games, games.size());
  CHECK_EQ(report.scored, games.size() - 96);
  CHECK_EQ(report.rows.size(), predictors.size());
  CHECK_EQ(report.rows.front().name, std::string("Elo"));
  for (const auto& row : report.rows) CHECK_EQ(row.card.games(), report.scored);

  const auto& elo = report.rows[0].card;
  const auto& home = report.rows[2].card;
  CHECK(elo.accuracy() > 0.95);
  CHECK_NEAR(home.accuracy(), 0.5, 1e-9);
  CHECK(elo.brier() < 0.3);
}

TEST_CASE(backtest_report_formats) {
  const auto games = ranked_league();
  const auto predictors = make_predictors(default_config(Sport::Nba));
  const auto report = run_backtest(Sport::Nba, games, predictors, {.warmup_days = 0, .min_games = 0});
  const std::string md = format_markdown(report);
  CHECK(md.find("| Elo |") != std::string::npos);
  CHECK(md.find("| Always home | 50.0% | n/a | n/a |") != std::string::npos);

  const Json json = to_json(report);
  CHECK_EQ(json.get_string("sport"), std::string("nba"));
  CHECK_EQ(json.get_number("scored").value(), static_cast<double>(games.size()));
}

TEST_CASE(backtest_handles_no_games) {
  const auto predictors = make_predictors(default_config(Sport::Nhl));
  const auto report = run_backtest(Sport::Nhl, {}, predictors, {});
  CHECK_EQ(report.scored, std::size_t{0});
  CHECK(format_markdown(report).find("0 games scored") != std::string::npos);
}
