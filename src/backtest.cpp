#include "picks/backtest.hpp"

#include <cmath>
#include <cstdio>

namespace picks {
namespace {

std::string fixed(double v, int decimals) {
  char buf[32];
  std::snprintf(buf, sizeof buf, "%.*f", decimals, v);
  return buf;
}

std::string with_commas(std::size_t n) {
  std::string digits = std::to_string(n);
  for (std::size_t i = digits.size(); i > 3; i -= 3) digits.insert(i - 3, ",");
  return digits;
}

double round_to(double v, int decimals) {
  const double scale = std::pow(10.0, decimals);
  return std::round(v * scale) / scale;
}

}  // namespace

BacktestReport run_backtest(Sport sport, const std::vector<Game>& games,
                            const std::vector<std::unique_ptr<Predictor>>& predictors,
                            const BacktestOptions& options) {
  BacktestReport report;
  report.sport = sport;
  report.games = games.size();
  for (const auto& p : predictors) report.rows.push_back({std::string(p->name()), p->probabilistic(), {}});
  if (games.empty() || predictors.empty()) return report;

  report.first = games.front().start;
  report.last = games.back().start;
  report.scored_from = report.first + Timestamp{options.warmup_days} * kSecondsPerDay;
  const Predictor& model = *predictors.front();

  for (const Game& game : games) {
    const bool scored = game.start >= report.scored_from &&
                        model.games_played(game.home) >= options.min_games &&
                        model.games_played(game.away) >= options.min_games;
    if (scored) {
      ++report.scored;
      for (std::size_t i = 0; i < predictors.size(); ++i) {
        report.rows[i].card.add(predictors[i]->predict(game), game.outcome());
      }
    }
    for (const auto& p : predictors) p->observe(game);
  }
  return report;
}

std::string format_markdown(const BacktestReport& r) {
  std::string out = "### " + std::string(sport_name(r.sport)) + ": " + with_commas(r.scored) +
                    " games scored";
  if (r.scored > 0) {
    out += " (" + format_iso8601(r.scored_from).substr(0, 10) + " to " +
           format_iso8601(r.last).substr(0, 10) + ", after " + with_commas(r.games - r.scored) +
           " training-only games)";
  }
  out += "\n\n| Model | Accuracy | Brier | Log loss |\n|---|---:|---:|---:|\n";
  for (const BacktestRow& row : r.rows) {
    out += "| " + row.name + " | " + fixed(100.0 * row.card.accuracy(), 1) + "% | ";
    out += row.probabilistic ? fixed(row.card.brier(), 4) + " | " + fixed(row.card.log_loss(), 4)
                             : std::string("n/a | n/a");
    out += " |\n";
  }
  return out;
}

Json to_json(const BacktestReport& r) {
  Json::Array rows;
  for (const BacktestRow& row : r.rows) {
    Json::Object o{{"model", row.name}, {"accuracy", round_to(row.card.accuracy(), 4)}};
    if (row.probabilistic) {
      o.emplace_back("brier", round_to(row.card.brier(), 4));
      o.emplace_back("logLoss", round_to(row.card.log_loss(), 4));
    }
    rows.emplace_back(std::move(o));
  }
  return Json::Object{
      {"sport", std::string(sport_name(r.sport))},
      {"games", r.games},
      {"scored", r.scored},
      {"from", format_iso8601(r.scored_from)},
      {"to", format_iso8601(r.last)},
      {"rows", std::move(rows)},
  };
}

}  // namespace picks
