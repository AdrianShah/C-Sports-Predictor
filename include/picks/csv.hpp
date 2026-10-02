#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace picks {

struct CsvError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

using CsvRow = std::vector<std::string>;

/// RFC 4180 parsing: quoted fields, "" escapes, commas and newlines inside quotes,
/// LF or CRLF line endings, optional UTF-8 BOM. Blank lines are skipped.
std::vector<CsvRow> parse_csv(std::string_view text);

/// A CSV file whose first row is the header. Every row must match the header width.
class CsvTable {
 public:
  explicit CsvTable(std::vector<CsvRow> rows);

  const CsvRow& header() const noexcept { return header_; }
  const std::vector<CsvRow>& rows() const noexcept { return rows_; }

  std::optional<std::size_t> column(std::string_view name) const noexcept;
  /// Like column(), but throws CsvError when the column is missing.
  std::size_t require_column(std::string_view name) const;

 private:
  CsvRow header_;
  std::vector<CsvRow> rows_;
};

CsvTable read_csv_file(const std::filesystem::path& path);

}  // namespace picks
