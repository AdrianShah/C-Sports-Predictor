// picks-model: rating models that predict the same games I pick.
//
//   picks-model backtest --history history [--sport all|soccer|nba|nhl]
//   picks-model predict  --history history --fixtures fixtures.json --out model_picks.json
//   picks-model ratings  --history history --sport nba

#include <charconv>
#include <chrono>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "picks/backtest.hpp"
#include "picks/config.hpp"
#include "picks/history.hpp"
#include "picks/io.hpp"
#include "picks/json.hpp"
#include "picks/predict.hpp"

namespace {

using namespace picks;
namespace fs = std::filesystem;

constexpr std::string_view kUsage = R"(picks-model )" PICKS_VERSION R"(

Usage:
  picks-model backtest --history DIR [--sport all|soccer|nba|nhl]
                       [--warmup-days N] [--min-games N] [--set KEY=VALUE]... [--json FILE]
  picks-model predict  --history DIR --fixtures FILE --out FILE
                       [--now ISO-8601] [--min-games N] [--config FILE]
                       [--exclude TEXT]... [--set KEY=VALUE]...
  picks-model ratings  --history DIR --sport soccer|nba|nhl [--top N] [--min-games N]

History is read from DIR/<sport>/*.csv (see tools/backfill.mjs).
--set overrides a model parameter, for every sport or one ("nba:elo.k=24").
Keys: elo.{k,home_advantage,carry_over,offseason_days,draw_rate,initial}
      goals.{learning_rate,prior_games,global_rate,rho,carry_over,offseason_days,max_goals}
)";

struct UsageError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

/// Default competition filter for predict (also in config/competitions.json).
const std::vector<std::string> kDefaultExclusions = {
    "women", "femen", "frauen", "femin", "fémin", "vrouwen", "damallsvenskan",
    "nwsl", "ncaa", "shebelieves", "pinatar",
    "=spanish liga f", "=french première ligue", "=northern super league", "=usl super league"};

class Args {
 public:
  Args(int argc, char** argv) {
    for (int i = 2; i < argc; ++i) {
      const std::string_view arg = argv[i];
      if (!arg.starts_with("--")) throw UsageError("unexpected argument '" + std::string(arg) + "'");
      const bool has_value = i + 1 < argc && !std::string_view(argv[i + 1]).starts_with("--");
      options_[std::string(arg.substr(2))].push_back(has_value ? argv[++i] : "");
    }
  }

  std::optional<std::string> get(std::string_view key) const {
    const auto it = options_.find(key);
    if (it == options_.end()) return std::nullopt;
    return it->second.back();
  }
  std::string require(std::string_view key) const {
    auto v = get(key);
    if (!v || v->empty()) throw UsageError("missing --" + std::string(key));
    return *v;
  }
  std::vector<std::string> all(std::string_view key) const {
    const auto it = options_.find(key);
    return it == options_.end() ? std::vector<std::string>{} : it->second;
  }
  int get_int(std::string_view key, int fallback) const {
    const auto v = get(key);
    if (!v) return fallback;
    int out = 0;
    const auto [ptr, ec] = std::from_chars(v->data(), v->data() + v->size(), out);
    if (ec != std::errc{} || ptr != v->data() + v->size() || out < 0) {
      throw UsageError("--" + std::string(key) + " needs a whole number");
    }
    return out;
  }

 private:
  std::map<std::string, std::vector<std::string>, std::less<>> options_;
};

std::vector<Sport> sports_arg(const Args& args, bool allow_all) {
  const std::string name = args.get("sport").value_or(allow_all ? "all" : "");
  if (allow_all && name == "all") return {kAllSports.begin(), kAllSports.end()};
  if (const auto sport = parse_sport(name)) return {*sport};
  throw UsageError("--sport must be " + std::string(allow_all ? "all, " : "") + "soccer, nba or nhl");
}

SportConfig config_for(Sport sport, const Args& args) {
  SportConfig config = default_config(sport);
  for (const std::string& setting : args.all("set")) {
    std::string_view s = setting;
    if (const auto colon = s.find(':'); colon != std::string_view::npos) {
      if (s.substr(0, colon) != sport_name(sport)) continue;  // meant for another sport
      s.remove_prefix(colon + 1);
    }
    apply_setting(config, s);
  }
  return config;
}

Timestamp now_utc() {
  using namespace std::chrono;
  return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

/// Trains the model of record for `sport` on all of its history.
std::unique_ptr<Predictor> train(Sport sport, const std::vector<Game>& games, const Args& args) {
  auto predictors = make_predictors(config_for(sport, args));
  std::unique_ptr<Predictor> model = std::move(predictors.front());
  for (const Game& g : games) model->observe(g);
  return model;
}

int cmd_backtest(const Args& args) {
  const fs::path history = args.require("history");
  const auto sports = sports_arg(args, true);
  const BacktestOptions options{.warmup_days = args.get_int("warmup-days", 365),
                                .min_games = args.get_int("min-games", 10)};

  Json::Array reports;
  for (const Sport sport : sports) {
    if (sports.size() > 1 && !fs::is_directory(history / sport_name(sport))) {
      std::cerr << "skipping " << sport_name(sport) << ": no history\n";
      continue;
    }
    const auto games = load_history(history, sport);
    const auto predictors = make_predictors(config_for(sport, args));
    const BacktestReport report = run_backtest(sport, games, predictors, options);
    std::cout << format_markdown(report) << '\n';
    reports.push_back(to_json(report));
  }
  if (const auto out = args.get("json"); out && !out->empty()) {
    write_file(*out, Json(std::move(reports)).dump(2) + "\n");
  }
  return 0;
}

int cmd_predict(const Args& args) {
  const fs::path history = args.require("history");
  const fs::path fixtures_path = args.require("fixtures");
  const fs::path out_path = args.require("out");

  PredictOptions options;
  options.min_games = args.get_int("min-games", 10);
  options.now = now_utc();
  if (const auto now = args.get("now")) {
    const auto parsed = parse_iso8601(*now);
    if (!parsed) throw UsageError("--now must be an ISO-8601 time");
    options.now = *parsed;
  }
  if (const auto config = args.get("config")) {
    const Json doc = Json::parse(read_file(*config));
    if (const Json* list = doc.find("excludeCompetitions"); list && list->is_array()) {
      for (const Json& item : list->as_array()) options.exclude_competitions.push_back(item.as_string());
    }
  } else {
    options.exclude_competitions = kDefaultExclusions;
  }
  for (const std::string& extra : args.all("exclude")) options.exclude_competitions.push_back(extra);

  const auto fixtures = read_fixtures(Json::parse(read_file(fixtures_path)));
  std::vector<ModelPick> previous;
  if (fs::exists(out_path)) previous = read_model_picks(Json::parse(read_file(out_path)));

  std::vector<ModelPick> fresh;
  for (const Sport sport : kAllSports) {
    if (!fs::is_directory(history / sport_name(sport))) {
      std::cerr << sport_name(sport) << ": no history, skipped\n";
      continue;
    }
    // Only games that finished before `now` may inform a pick.
    auto games = load_history(history, sport);
    std::erase_if(games, [&](const Game& g) { return g.start >= options.now; });
    const auto model = train(sport, games, args);

    PredictStats stats;
    auto picks = predict_fixtures(*model, sport, fixtures, options, stats);
    std::cerr << sport_name(sport) << ": " << stats.picked << " picks from " << stats.upcoming
              << " upcoming (" << stats.excluded << " excluded, " << stats.unknown_teams
              << " with too little history), trained on " << games.size() << " games\n";
    fresh.insert(fresh.end(), std::make_move_iterator(picks.begin()), std::make_move_iterator(picks.end()));
  }

  const auto merged = merge_picks(std::move(previous), std::move(fresh), options.now);
  write_file(out_path, write_model_picks(merged, options.now));
  std::cerr << "wrote " << merged.size() << " picks to " << out_path.string() << '\n';
  return 0;
}

int cmd_ratings(const Args& args) {
  const fs::path history = args.require("history");
  const Sport sport = sports_arg(args, false).front();
  const int top = args.get_int("top", 25);
  const int min_games = args.get_int("min-games", 10);

  const auto games = load_history(history, sport);
  EloModel elo(config_for(sport, args).elo);
  for (const Game& g : games) elo.observe(g);

  const Timestamp at = games.empty() ? 0 : games.back().start;
  int rank = 0;
  for (const TeamRating& t : elo.table(at)) {
    if (t.games < min_games) continue;
    if (++rank > top) break;
    char line[64];
    std::snprintf(line, sizeof line, "%3d  %7.1f  %5d  ", rank, t.rating, t.games);
    std::cout << line << t.team << '\n';
  }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string_view command = argc > 1 ? argv[1] : "";
  try {
    if (command.empty() || command == "--help" || command == "-h" || command == "help") {
      std::cout << kUsage;
      return command.empty() ? 2 : 0;
    }
    if (command == "--version") {
      std::cout << "picks-model " PICKS_VERSION "\n";
      return 0;
    }
    const Args args(argc, argv);
    if (command == "backtest") return cmd_backtest(args);
    if (command == "predict") return cmd_predict(args);
    if (command == "ratings") return cmd_ratings(args);
    throw UsageError("unknown command '" + std::string(command) + "'");
  } catch (const UsageError& e) {
    std::cerr << "error: " << e.what() << "\n\n" << kUsage;
    return 2;
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << '\n';
    return 1;
  }
}
