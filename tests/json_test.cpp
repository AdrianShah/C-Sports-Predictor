#include "check.hpp"
#include "picks/json.hpp"

using namespace picks;

TEST_CASE(json_parses_nested_values) {
  const Json doc = Json::parse(R"( {"events": [ {"id": "espn:1", "n": -1.5e2, "ok": true, "x": null} ]} )");
  const Json* events = doc.find("events");
  CHECK(events && events->is_array());
  const Json& e = events->as_array().at(0);
  CHECK_EQ(e.get_string("id"), std::string("espn:1"));
  CHECK_EQ(e.get_number("n").value(), -150.0);
  CHECK(e.find("ok")->as_bool());
  CHECK(e.find("x")->is_null());
  CHECK(!e.find("missing"));
  CHECK_EQ(e.get_string("missing", "fallback"), std::string("fallback"));
}

TEST_CASE(json_decodes_escapes_to_utf8) {
  CHECK_EQ(Json::parse(R"("Atl\u00e9tico")").as_string(), std::string("Atl\xC3\xA9tico"));
  CHECK_EQ(Json::parse(R"("\ud83d\ude00")").as_string(), std::string("\xF0\x9F\x98\x80"));
  CHECK_EQ(Json::parse(R"("a\"b\\c\/d\n")").as_string(), std::string("a\"b\\c/d\n"));
  CHECK_EQ(Json::parse("\"Olympiac\xC3\xB3s\"").as_string(), std::string("Olympiac\xC3\xB3s"));
}

TEST_CASE(json_round_trips_compact) {
  const std::string text = R"({"b":[1,2.5,true,null],"a":"x\"y","c":{}})";
  CHECK_EQ(Json::parse(text).dump(), text);  // key order is preserved
}

TEST_CASE(json_pretty_prints) {
  const Json doc = Json::Object{{"a", 1}, {"b", Json::Array{true}}};
  CHECK_EQ(doc.dump(2), std::string("{\n  \"a\": 1,\n  \"b\": [\n    true\n  ]\n}"));
}

TEST_CASE(json_rejects_malformed_input) {
  for (const char* bad : {"", "{", "[1,]", "tru", "\"abc", "1 2", "01", "-", "1.", "{\"a\" 1}",
                          "\"\\x\"", "\"\\ud83d\"", "[\"\x01\"]", "nul"}) {
    bool threw = false;
    try {
      Json::parse(bad);
    } catch (const JsonError&) {
      threw = true;
    }
    if (!threw) ::check::fail(__FILE__, __LINE__, std::string("accepted '") + bad + "'");
  }
}

TEST_CASE(json_limits_nesting_depth) {
  CHECK_THROWS(Json::parse(std::string(10'000, '[')), JsonError);
  CHECK(Json::parse(std::string(100, '[') + std::string(100, ']')).is_array());
}

TEST_CASE(json_type_errors_throw) {
  const Json n = 3;
  CHECK_THROWS(n.as_string(), JsonError);
  CHECK_EQ(n.as_number(), 3.0);
}
