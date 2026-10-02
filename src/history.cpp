#include "picks/history.hpp"

#include <algorithm>
#include <charconv>
#include <iterator>
#include <string>
#include <unordered_set>

namespace picks {
namespace {

std::optional<int> parse_int(std::string_view s) {
  int v = 0;
  const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
  if (ec != std::errc{} || ptr != s.data() + s.size()) return std::nullopt;
  return v;
}

}  // namespace

std::vector<Game> games_from_csv(const CsvTable& table, std::string_view source) {
  const std::size_t c_date = table.require_column("date");
  const std::size_t c_home = table.require_column("home");
  const std::size_t c_away = table.require_column("away");
  const std::size_t c_hs = table.require_column("home_score");
  const std::size_t c_as = table.require_column("away_score");
  const auto c_season = table.column("season");
  const auto c_league = table.column("league");
  const auto c_neutral = table.column("neutral");
  const auto c_id = table.column("espn_id");

  std::vector<Game> games;
  games.reserve(table.rows().size());
  for (std::size_t i = 0; i < table.rows().size(); ++i) {
    const CsvRow& row = table.rows()[i];
    const auto bad = [&](std::string_view column, const std::string& value) {
      return HistoryError(std::string(source) + ":" + std::to_string(i + 2) + ": bad " +
                          std::string(column) + " '" + value + "'");
    };

    Game g;
    const auto start = parse_iso8601(row[c_date]);
    if (!start) throw bad("date", row[c_date]);
    g.start = *start;
    const auto hs = parse_int(row[c_hs]);
    const auto as = parse_int(row[c_as]);
    if (!hs || *hs < 0) throw bad("home_score", row[c_hs]);
    if (!as || *as < 0) throw bad("away_score", row[c_as]);
    g.home_score = *hs;
    g.away_score = *as;
    g.home = row[c_home];
    g.away = row[c_away];
    if (g.home.empty() || g.away.empty()) throw bad("team", "");
    if (c_season && !row[*c_season].empty()) {
      const auto season = parse_int(row[*c_season]);
      if (!season) throw bad("season", row[*c_season]);
      g.season = *season;
    }
    if (c_league) g.league = row[*c_league];
    if (c_neutral) g.neutral = row[*c_neutral] == "1" || row[*c_neutral] == "true";
    if (c_id) g.id = row[*c_id];
    games.push_back(std::move(g));
  }
  return games;
}

std::vector<Game> load_history(const std::filesystem::path& root, Sport sport) {
  const std::filesystem::path dir = root / sport_name(sport);
  if (!std::filesystem::is_directory(dir)) {
    throw HistoryError("no history for " + std::string(sport_name(sport)) + " (expected " +
                       dir.string() + ")");
  }

  std::vector<std::filesystem::path> files;
  for (const auto& entry : std::filesystem::directory_iterator(dir)) {
    if (entry.is_regular_file() && entry.path().extension() == ".csv") files.push_back(entry.path());
  }
  std::sort(files.begin(), files.end());

  std::vector<Game> games;
  for (const auto& file : files) {
    auto chunk = games_from_csv(read_csv_file(file), file.string());
    games.insert(games.end(), std::make_move_iterator(chunk.begin()),
                 std::make_move_iterator(chunk.end()));
  }
  std::stable_sort(games.begin(), games.end(),
                   [](const Game& a, const Game& b) { return a.start < b.start; });

  std::unordered_set<std::string> seen;
  std::erase_if(games, [&seen](const Game& g) { return !g.id.empty() && !seen.insert(g.id).second; });
  return games;
}

}  // namespace picks
