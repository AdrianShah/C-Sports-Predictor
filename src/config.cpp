#include "picks/config.hpp"

#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <string>

#include "picks/baselines.hpp"

namespace picks {
namespace {

double parse_double(std::string_view text, std::string_view key) {
  const std::string s(text);
  char* end = nullptr;
  const double v = std::strtod(s.c_str(), &end);
  if (s.empty() || end != s.c_str() + s.size() || !std::isfinite(v)) {
    throw std::invalid_argument("bad value for " + std::string(key) + ": '" + s + "'");
  }
  return v;
}

int to_int(double v, std::string_view key) {
  if (v != std::floor(v) || v < 0 || v > 1e6) {
    throw std::invalid_argument(std::string(key) + " must be a whole number");
  }
  return static_cast<int>(v);
}

}  // namespace

SportConfig default_config(Sport sport) {
  SportConfig c;
  c.sport = sport;
  switch (sport) {
    case Sport::Nba:
      c.elo = {.k = 20.0, .home_advantage = 50.0, .carry_over = 0.75, .offseason_days = 60,
               .draw_rate = 0.0, .margin = MarginRule::Basketball};
      break;
    case Sport::Nhl:
      c.elo = {.k = 8.0, .home_advantage = 30.0, .carry_over = 0.7, .offseason_days = 60,
               .draw_rate = 0.0, .margin = MarginRule::Logarithmic};
      break;
    case Sport::Soccer:
      c.elo = {.k = 20.0, .home_advantage = 65.0, .carry_over = 0.8, .offseason_days = 75,
               .draw_rate = 0.28, .margin = MarginRule::Logarithmic};
      c.goals = GoalsParams{};
      break;
  }
  return c;
}

void apply_setting(SportConfig& c, std::string_view assignment) {
  const auto eq = assignment.find('=');
  if (eq == std::string_view::npos) {
    throw std::invalid_argument("expected key=value, got '" + std::string(assignment) + "'");
  }
  const std::string_view key = assignment.substr(0, eq);
  const double v = parse_double(assignment.substr(eq + 1), key);

  if (key == "elo.initial") c.elo.initial = v;
  else if (key == "elo.k") c.elo.k = v;
  else if (key == "elo.home_advantage") c.elo.home_advantage = v;
  else if (key == "elo.carry_over") c.elo.carry_over = v;
  else if (key == "elo.offseason_days") c.elo.offseason_days = to_int(v, key);
  else if (key == "elo.draw_rate") c.elo.draw_rate = v;
  else if (key == "goals.learning_rate") c.goals.learning_rate = v;
  else if (key == "goals.prior_games") c.goals.prior_games = v;
  else if (key == "goals.global_rate") c.goals.global_rate = v;
  else if (key == "goals.rho") c.goals.rho = v;
  else if (key == "goals.carry_over") c.goals.carry_over = v;
  else if (key == "goals.offseason_days") c.goals.offseason_days = to_int(v, key);
  else if (key == "goals.max_goals") c.goals.max_goals = to_int(v, key);
  else throw std::invalid_argument("unknown setting '" + std::string(key) + "'");
}

std::vector<std::unique_ptr<Predictor>> make_predictors(const SportConfig& c) {
  std::vector<std::unique_ptr<Predictor>> out;
  if (c.sport == Sport::Soccer) out.push_back(std::make_unique<GoalsModel>(c.goals));
  out.push_back(std::make_unique<EloModel>(c.elo));
  out.push_back(std::make_unique<BetterRecord>());
  out.push_back(std::make_unique<AlwaysHome>());
  out.push_back(std::make_unique<BaseRate>(allows_draw(c.sport)));
  return out;
}

}  // namespace picks
