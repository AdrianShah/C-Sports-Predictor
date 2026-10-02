#include "picks/csv.hpp"

#include "picks/io.hpp"

namespace picks {

std::vector<CsvRow> parse_csv(std::string_view text) {
  if (text.starts_with("\xEF\xBB\xBF")) text.remove_prefix(3);

  std::vector<CsvRow> rows;
  CsvRow row;
  std::string field;
  bool in_quotes = false;
  bool quoted = false;  // current field started with a quote

  const auto end_field = [&] {
    row.push_back(std::move(field));
    field.clear();
    quoted = false;
  };
  const auto end_row = [&] {
    const bool blank_line = row.empty() && field.empty() && !quoted;
    end_field();
    if (!blank_line) rows.push_back(std::move(row));
    row.clear();
  };

  for (std::size_t i = 0; i < text.size(); ++i) {
    const char c = text[i];
    if (in_quotes) {
      if (c != '"') {
        field += c;
      } else if (i + 1 < text.size() && text[i + 1] == '"') {
        field += '"';
        ++i;
      } else {
        in_quotes = false;
      }
      continue;
    }
    switch (c) {
      case '"':
        if (field.empty() && !quoted) {
          in_quotes = quoted = true;
        } else {
          field += c;  // stray quote inside an unquoted field: keep it
        }
        break;
      case ',':
        end_field();
        break;
      case '\r':
        if (i + 1 < text.size() && text[i + 1] == '\n') break;  // the '\n' ends the row
        end_row();
        break;
      case '\n':
        end_row();
        break;
      default:
        field += c;
    }
  }
  if (in_quotes) throw CsvError("CSV: unterminated quoted field");
  if (!field.empty() || quoted || !row.empty()) end_row();
  return rows;
}

CsvTable::CsvTable(std::vector<CsvRow> rows) {
  if (rows.empty()) throw CsvError("CSV: missing header row");
  header_ = std::move(rows.front());
  rows_.reserve(rows.size() - 1);
  for (std::size_t i = 1; i < rows.size(); ++i) {
    if (rows[i].size() != header_.size()) {
      throw CsvError("CSV: line " + std::to_string(i + 1) + " has " +
                     std::to_string(rows[i].size()) + " fields, expected " +
                     std::to_string(header_.size()));
    }
    rows_.push_back(std::move(rows[i]));
  }
}

std::optional<std::size_t> CsvTable::column(std::string_view name) const noexcept {
  for (std::size_t i = 0; i < header_.size(); ++i) {
    if (header_[i] == name) return i;
  }
  return std::nullopt;
}

std::size_t CsvTable::require_column(std::string_view name) const {
  if (const auto index = column(name)) return *index;
  throw CsvError("CSV: missing column '" + std::string(name) + "'");
}

CsvTable read_csv_file(const std::filesystem::path& path) {
  try {
    return CsvTable(parse_csv(read_file(path)));
  } catch (const CsvError& e) {
    throw CsvError(path.string() + ": " + e.what());
  }
}

}  // namespace picks
