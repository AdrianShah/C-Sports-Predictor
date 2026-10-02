#include "picks/io.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace picks {

std::string read_file(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("cannot open " + path.string());
  std::ostringstream buffer;
  buffer << in.rdbuf();
  return std::move(buffer).str();
}

void write_file(const std::filesystem::path& path, std::string_view contents) {
  if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
  std::filesystem::path tmp = path;
  tmp += ".tmp";
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("cannot write " + tmp.string());
    out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    if (!out) throw std::runtime_error("failed writing " + tmp.string());
  }
  std::filesystem::rename(tmp, path);
}

}  // namespace picks
