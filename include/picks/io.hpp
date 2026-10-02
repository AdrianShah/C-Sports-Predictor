#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace picks {

/// Reads a whole file; throws std::runtime_error if it can't be opened.
std::string read_file(const std::filesystem::path& path);

/// Writes via a temporary file and a rename, so readers never see half a file.
void write_file(const std::filesystem::path& path, std::string_view contents);

}  // namespace picks
