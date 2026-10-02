#include "check.hpp"
#include "picks/csv.hpp"

using namespace picks;

TEST_CASE(csv_parses_plain_rows) {
  const auto rows = parse_csv("a,b\n1,2\n");
  CHECK_EQ(rows.size(), std::size_t{2});
  CHECK_EQ(rows[1][0], std::string("1"));
  CHECK_EQ(rows[1][1], std::string("2"));
}

TEST_CASE(csv_handles_quotes_and_crlf) {
  const auto rows = parse_csv("name,note\r\n\"Paris, SG\",\"say \"\"hi\"\"\"\r\nx,\r\n");
  CHECK_EQ(rows.size(), std::size_t{3});
  CHECK_EQ(rows[1][0], std::string("Paris, SG"));
  CHECK_EQ(rows[1][1], std::string("say \"hi\""));
  CHECK_EQ(rows[2][0], std::string("x"));
  CHECK_EQ(rows[2][1], std::string(""));
}

TEST_CASE(csv_keeps_newlines_inside_quotes) {
  const auto rows = parse_csv("a\n\"line 1\nline 2\"");
  CHECK_EQ(rows.size(), std::size_t{2});
  CHECK_EQ(rows[1][0], std::string("line 1\nline 2"));
}

TEST_CASE(csv_skips_bom_and_blank_lines) {
  const auto rows = parse_csv("\xEF\xBB\xBF" "date,x\n\n1,2");
  CHECK_EQ(rows.size(), std::size_t{2});
  CHECK_EQ(rows[0][0], std::string("date"));
}

TEST_CASE(csv_keeps_quoted_empty_field) {
  const auto rows = parse_csv("a\n\"\"\n");
  CHECK_EQ(rows.size(), std::size_t{2});
  CHECK_EQ(rows[1][0], std::string(""));
}

TEST_CASE(csv_rejects_unterminated_quote) { CHECK_THROWS(parse_csv("a\n\"open"), CsvError); }

TEST_CASE(csv_table_columns) {
  const CsvTable table(parse_csv("date,home\n2025-01-01,Raptors\n"));
  CHECK_EQ(table.column("home").value(), std::size_t{1});
  CHECK(!table.column("away").has_value());
  CHECK_THROWS(table.require_column("away"), CsvError);
  CHECK_THROWS(CsvTable(parse_csv("a,b\n1\n")), CsvError);
  CHECK_THROWS(CsvTable(parse_csv("")), CsvError);
}
