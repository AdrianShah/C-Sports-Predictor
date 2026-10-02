#include "check.hpp"
#include "picks/elo.hpp"
#include "picks/predict.hpp"

using namespace picks;

namespace {

Timestamp at(const char* iso) { return parse_iso8601(iso).value(); }

const char* kFixtures = R"({"syncedAt":"2026-01-01T00:00:00Z","events":[
{"id":"espn:basketball:nba:1","sport":"nba","competition":"NBA","a":"Raptors","b":"Heat","scheduledAt":"2026-01-02T00:00:00.000Z","status":"upcoming"},
{"id":"espn:basketball:nba:2","sport":"nba","competition":"NBA","a":"Heat","b":"Raptors","scheduledAt":"2025-12-30T00:00:00.000Z","status":"finished"},
{"id":"espn:basketball:nba:3","sport":"nba","competition":"NBA","a":"Raptors","b":"Expansion","scheduledAt":"2026-01-03T00:00:00.000Z","status":"upcoming"},
{"id":"espn:soccer:all:4","sport":"soccer","competition":"English Women's Super League","a":"Chelsea","b":"Arsenal","scheduledAt":"2026-01-03T00:00:00.000Z","status":"upcoming"},
{"id":"apib:5","sport":"basketball","competition":"Greek Basket League","a":"Olympiacos","b":"PAO","scheduledAt":"2026-01-03T00:00:00.000Z","status":"upcoming"},
{"id":"espn:basketball:nba:6","sport":"nba","competition":"NBA","a":"Heat","b":"Raptors","scheduledAt":"not a date","status":"upcoming"}
]})";

ModelPick pick(std::string id, const char* scheduled, const char* locked, std::string choice) {
  ModelPick p;
  p.event_id = std::move(id);
  p.sport = "nba";
  p.home = "Raptors";
  p.away = "Heat";
  p.scheduled_at = at(scheduled);
  p.locked_at = at(locked);
  p.pick = std::move(choice);
  p.probs = {0.6, 0.0, 0.4};
  return p;
}

}  // namespace

TEST_CASE(predict_reads_supported_fixtures) {
  const auto fixtures = read_fixtures(Json::parse(kFixtures));
  CHECK_EQ(fixtures.size(), std::size_t{4});  // skips the "basketball" sport and the bad date
  CHECK_EQ(fixtures[0].home, std::string("Raptors"));
  CHECK(fixtures[3].sport == Sport::Soccer);
  CHECK_THROWS(read_fixtures(Json::parse("{}")), JsonError);
}

TEST_CASE(predict_picks_upcoming_known_games) {
  EloModel elo({.home_advantage = 0});
  for (int d = 0; d < 12; ++d) {
    elo.observe({.start = d * kSecondsPerDay, .home = "Heat", .away = "Raptors",
                 .home_score = 90, .away_score = 110});
  }
  const auto fixtures = read_fixtures(Json::parse(kFixtures));
  PredictOptions options{.now = at("2026-01-01T00:00:00Z"), .min_games = 10, .exclude_competitions = {"women"}};
  PredictStats stats;

  const auto nba = predict_fixtures(elo, Sport::Nba, fixtures, options, stats);
  CHECK_EQ(nba.size(), std::size_t{1});
  CHECK_EQ(nba[0].event_id, std::string("espn:basketball:nba:1"));
  CHECK_EQ(nba[0].pick, std::string("Raptors"));
  CHECK_EQ(nba[0].locked_at, options.now);
  CHECK(nba[0].locked_at < nba[0].scheduled_at);
  CHECK_EQ(stats.upcoming, std::size_t{2});
  CHECK_EQ(stats.unknown_teams, std::size_t{1});

  PredictStats soccer_stats;
  CHECK(predict_fixtures(elo, Sport::Soccer, fixtures, options, soccer_stats).empty());
  CHECK_EQ(soccer_stats.excluded, std::size_t{1});
}

TEST_CASE(competition_filter_patterns) {
  const std::vector<std::string> patterns = {"women", "=spanish liga f"};
  CHECK(competition_excluded("UEFA Women's Champions League", patterns));
  CHECK(competition_excluded("Spanish Liga F", patterns));
  CHECK(!competition_excluded("Costa Rican Liga FPD", patterns));
  CHECK(!competition_excluded("English Premier League", patterns));
  CHECK(!competition_excluded("anything", {}));
}

TEST_CASE(merge_freezes_started_games) {
  const Timestamp now = at("2026-01-02T12:00:00Z");
  std::vector<ModelPick> previous = {
      pick("started", "2026-01-02T00:00:00Z", "2026-01-01T00:00:00Z", "Raptors"),
      pick("same", "2026-01-03T00:00:00Z", "2026-01-01T00:00:00Z", "Raptors"),
      pick("changed", "2026-01-04T00:00:00Z", "2026-01-01T00:00:00Z", "Raptors"),
      pick("dropped", "2026-01-05T00:00:00Z", "2026-01-01T00:00:00Z", "Heat"),
  };
  std::vector<ModelPick> fresh = {
      pick("started", "2026-01-06T00:00:00Z", "2026-01-02T12:00:00Z", "Heat"),  // postponed
      pick("same", "2026-01-03T00:00:00Z", "2026-01-02T12:00:00Z", "Raptors"),
      pick("changed", "2026-01-04T00:00:00Z", "2026-01-02T12:00:00Z", "Heat"),
      pick("new", "2026-01-03T06:00:00Z", "2026-01-02T12:00:00Z", "Heat"),
  };
  const auto merged = merge_picks(previous, fresh, now);

  CHECK_EQ(merged.size(), std::size_t{5});
  CHECK_EQ(merged[0].event_id, std::string("started"));
  CHECK_EQ(merged[0].pick, std::string("Raptors"));
  CHECK_EQ(merged[1].event_id, std::string("same"));
  CHECK_EQ(merged[1].locked_at, at("2026-01-01T00:00:00Z"));
  CHECK_EQ(merged[2].event_id, std::string("new"));
  CHECK_EQ(merged[3].event_id, std::string("changed"));
  CHECK_EQ(merged[3].pick, std::string("Heat"));
  CHECK_EQ(merged[3].locked_at, now);
  CHECK_EQ(merged[4].event_id, std::string("dropped"));
}

TEST_CASE(model_picks_round_trip) {
  std::vector<ModelPick> picks = {pick("espn:basketball:nba:1", "2026-01-03T00:00:00Z",
                                       "2026-01-01T00:00:00Z", "Raptors")};
  picks[0].probs = {0.61234, 0.0, 0.38766};
  picks[0].home_logo = "https://a.espncdn.com/tor.png";
  const std::string text = write_model_picks(picks, at("2026-01-01T00:00:00Z"));
  CHECK(text.find("\"pA\":0.6123") != std::string::npos);
  CHECK(text.find("pDraw") == std::string::npos);  // two-way sports have no draw column
  CHECK(text.find("\"aLogo\":\"https://a.espncdn.com/tor.png\"") != std::string::npos);
  CHECK(text.find("bLogo") == std::string::npos);  // empty logos are left out

  const auto back = read_model_picks(Json::parse(text));
  CHECK_EQ(back.size(), std::size_t{1});
  CHECK_EQ(back[0].pick, std::string("Raptors"));
  CHECK_EQ(back[0].scheduled_at, picks[0].scheduled_at);
  CHECK_NEAR(back[0].probs.home, 0.6123, 1e-9);
  CHECK_EQ(back[0].home_logo, picks[0].home_logo);
  CHECK_THROWS(read_model_picks(Json::parse(R"({"picks":[{"eventId":"x"}]})")), JsonError);
}
