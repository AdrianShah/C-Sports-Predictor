#include <filesystem>

#include "check.hpp"
#include "picks/history.hpp"
#include "picks/io.hpp"

using namespace picks;
namespace fs = std::filesystem;

TEST_CASE(history_reads_columns_in_any_order) {
  const CsvTable table(parse_csv(
      "home_score,away_score,home,away,date\n"
      "2,1,Everton,Manchester United,2025-02-22T12:30Z\n"));
  const auto games = games_from_csv(table, "test.csv");
  CHECK_EQ(games.size(), std::size_t{1});
  CHECK_EQ(games[0].home, std::string("Everton"));
  CHECK_EQ(games[0].margin(), 1);
  CHECK(games[0].outcome() == Outcome::Home);
  CHECK(!games[0].neutral);
  CHECK_EQ(games[0].season, 0);
}

TEST_CASE(history_rejects_bad_rows) {
  const auto load = [](const char* csv) { return games_from_csv(CsvTable(parse_csv(csv)), "t.csv"); };
  CHECK_THROWS(load("date,home,away,home_score,away_score\nnot-a-date,A,B,1,0\n"), HistoryError);
  CHECK_THROWS(load("date,home,away,home_score,away_score\n2025-01-01,A,B,x,0\n"), HistoryError);
  CHECK_THROWS(load("date,home,away,home_score,away_score\n2025-01-01,A,B,-1,0\n"), HistoryError);
  CHECK_THROWS(load("date,home,away,home_score,away_score\n2025-01-01,,B,1,0\n"), HistoryError);
  CHECK_THROWS(load("date,home,away,home_score\n2025-01-01,A,B,1\n"), CsvError);
}

TEST_CASE(history_loads_sorted_and_deduplicated) {
  const fs::path root = fs::temp_directory_path() / "picks_model_history_test";
  fs::remove_all(root);
  const std::string header = "date,season,league,home,away,home_score,away_score,neutral,espn_id\n";
  write_file(root / "nba" / "2026.csv",
             header + "2026-01-05T00:00Z,2026,nba,C,D,100,90,0,3\n"
                      "2025-12-01T00:00Z,2026,nba,A,B,99,98,0,2\n");
  write_file(root / "nba" / "2025.csv",
             header + "2025-11-01T00:00Z,2026,nba,A,B,80,90,1,1\n"
                      "2025-12-01T00:00Z,2026,nba,A,B,99,98,0,2\n");
  write_file(root / "nba" / "notes.txt", "ignored");

  const auto games = load_history(root, Sport::Nba);
  CHECK_EQ(games.size(), std::size_t{3});
  CHECK_EQ(games[0].id, std::string("1"));
  CHECK(games[0].neutral);
  CHECK_EQ(games[2].id, std::string("3"));
  CHECK_THROWS(load_history(root, Sport::Nhl), HistoryError);
  fs::remove_all(root);
}
