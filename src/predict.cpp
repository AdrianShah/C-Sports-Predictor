#include "picks/predict.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

#include "picks/strings.hpp"

namespace picks {
namespace {

double round4(double v) { return std::round(v * 10'000.0) / 10'000.0; }

}  // namespace

bool competition_excluded(std::string_view competition, const std::vector<std::string>& patterns) {
  return std::any_of(patterns.begin(), patterns.end(), [&](const std::string& pattern) {
    if (pattern.starts_with('=')) return to_lower_ascii(competition) == to_lower_ascii(pattern.substr(1));
    return contains_ci(competition, pattern);
  });
}

std::vector<Fixture> read_fixtures(const Json& doc) {
  const Json* events = doc.find("events");
  if (!events || !events->is_array()) throw JsonError("fixtures: missing \"events\" array");

  std::vector<Fixture> out;
  for (const Json& e : events->as_array()) {
    const auto sport = parse_sport(e.get_string("sport"));
    const auto start = parse_iso8601(e.get_string("scheduledAt"));
    if (!sport || !start) continue;
    Fixture f{.id = e.get_string("id"),
              .sport = *sport,
              .competition = e.get_string("competition"),
              .home = e.get_string("a"),
              .away = e.get_string("b"),
              .start = *start,
              .status = e.get_string("status"),
              .home_logo = e.get_string("aLogo"),
              .away_logo = e.get_string("bLogo")};
    if (f.id.empty() || f.home.empty() || f.away.empty()) continue;
    out.push_back(std::move(f));
  }
  return out;
}

std::vector<ModelPick> read_model_picks(const Json& doc) {
  const Json* picks = doc.find("picks");
  if (!picks || !picks->is_array()) throw JsonError("model picks: missing \"picks\" array");

  std::vector<ModelPick> out;
  for (const Json& p : picks->as_array()) {
    ModelPick m;
    m.event_id = p.get_string("eventId");
    m.sport = p.get_string("sport");
    m.competition = p.get_string("competition");
    m.home = p.get_string("a");
    m.away = p.get_string("b");
    m.pick = p.get_string("pick");
    m.home_logo = p.get_string("aLogo");
    m.away_logo = p.get_string("bLogo");
    const auto scheduled = parse_iso8601(p.get_string("scheduledAt"));
    const auto locked = parse_iso8601(p.get_string("lockedAt"));
    if (m.event_id.empty() || m.pick.empty() || !scheduled || !locked) {
      throw JsonError("model picks: incomplete entry '" + m.event_id + "'");
    }
    m.scheduled_at = *scheduled;
    m.locked_at = *locked;
    m.probs = {p.get_number("pA").value_or(0.0), p.get_number("pDraw").value_or(0.0),
               p.get_number("pB").value_or(0.0)};
    out.push_back(std::move(m));
  }
  return out;
}

std::string write_model_picks(const std::vector<ModelPick>& picks, Timestamp generated_at) {
  std::string out = "{\"generatedAt\":\"" + format_iso8601(generated_at) +
                    "\",\"model\":\"picks-model " PICKS_VERSION "\",\"picks\":[";
  for (std::size_t i = 0; i < picks.size(); ++i) {
    const ModelPick& p = picks[i];
    Json::Object o{
        {"eventId", p.event_id},
        {"sport", p.sport},
        {"competition", p.competition},
        {"a", p.home},
        {"b", p.away},
        {"scheduledAt", format_iso8601(p.scheduled_at)},
        {"lockedAt", format_iso8601(p.locked_at)},
        {"pick", p.pick},
        {"pA", round4(p.probs.home)},
    };
    if (p.sport == "soccer") o.emplace_back("pDraw", round4(p.probs.draw));
    o.emplace_back("pB", round4(p.probs.away));
    if (!p.home_logo.empty()) o.emplace_back("aLogo", p.home_logo);
    if (!p.away_logo.empty()) o.emplace_back("bLogo", p.away_logo);
    out += i == 0 ? "\n" : ",\n";
    out += Json(std::move(o)).dump();
  }
  out += "\n]}\n";
  return out;
}

std::vector<ModelPick> predict_fixtures(const Predictor& model, Sport sport,
                                        const std::vector<Fixture>& fixtures,
                                        const PredictOptions& options, PredictStats& stats) {
  std::vector<ModelPick> out;
  for (const Fixture& f : fixtures) {
    if (f.sport != sport || f.status != "upcoming" || f.start <= options.now) continue;
    ++stats.upcoming;
    if (competition_excluded(f.competition, options.exclude_competitions)) {
      ++stats.excluded;
      continue;
    }
    if (model.games_played(f.home) < options.min_games || model.games_played(f.away) < options.min_games) {
      ++stats.unknown_teams;
      continue;
    }

    const Game game{.start = f.start, .home = f.home, .away = f.away};
    const Probs probs = model.predict(game);
    std::string pick = "draw";
    if (probs.pick() == Outcome::Home) pick = f.home;
    if (probs.pick() == Outcome::Away) pick = f.away;

    out.push_back({.event_id = f.id,
                   .sport = std::string(sport_name(sport)),
                   .competition = f.competition,
                   .home = f.home,
                   .away = f.away,
                   .scheduled_at = f.start,
                   .locked_at = options.now,
                   .pick = std::move(pick),
                   .probs = probs,
                   .home_logo = f.home_logo,
                   .away_logo = f.away_logo});
    ++stats.picked;
  }
  return out;
}

std::vector<ModelPick> merge_picks(std::vector<ModelPick> previous, std::vector<ModelPick> fresh,
                                   Timestamp now) {
  std::vector<ModelPick> out;
  std::unordered_map<std::string, const ModelPick*> open;  // previous picks still changeable
  std::unordered_set<std::string> taken;

  for (const ModelPick& p : previous) {
    if (p.scheduled_at <= now) {
      out.push_back(p);
      taken.insert(p.event_id);
    } else {
      open.emplace(p.event_id, &p);
    }
  }
  for (ModelPick& p : fresh) {
    if (!taken.insert(p.event_id).second) continue;
    if (const auto it = open.find(p.event_id); it != open.end() && it->second->pick == p.pick) {
      p.locked_at = it->second->locked_at;
    }
    out.push_back(std::move(p));
  }
  for (const auto& [id, p] : open) {
    if (taken.insert(id).second) out.push_back(*p);
  }

  std::sort(out.begin(), out.end(), [](const ModelPick& a, const ModelPick& b) {
    return a.scheduled_at != b.scheduled_at ? a.scheduled_at < b.scheduled_at : a.event_id < b.event_id;
  });
  return out;
}

}  // namespace picks
