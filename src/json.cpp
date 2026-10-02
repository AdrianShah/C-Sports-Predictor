#include "picks/json.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace picks {
namespace {

class Parser {
 public:
  explicit Parser(std::string_view text) : s_(text) {}

  Json document() {
    skip_ws();
    Json v = value(0);
    skip_ws();
    if (pos_ != s_.size()) fail("unexpected trailing characters");
    return v;
  }

 private:
  static constexpr int kMaxDepth = 256;

  [[noreturn]] void fail(const std::string& what) const {
    throw JsonError("JSON: " + what + " at offset " + std::to_string(pos_));
  }

  bool at_digit() const noexcept { return pos_ < s_.size() && s_[pos_] >= '0' && s_[pos_] <= '9'; }

  void skip_ws() noexcept {
    while (pos_ < s_.size() &&
           (s_[pos_] == ' ' || s_[pos_] == '\t' || s_[pos_] == '\n' || s_[pos_] == '\r')) {
      ++pos_;
    }
  }

  bool consume(char c) noexcept {
    if (pos_ < s_.size() && s_[pos_] == c) {
      ++pos_;
      return true;
    }
    return false;
  }

  void expect(char c) {
    if (!consume(c)) fail(std::string("expected '") + c + "'");
  }

  bool literal(std::string_view word) noexcept {
    if (s_.substr(pos_, word.size()) != word) return false;
    pos_ += word.size();
    return true;
  }

  Json value(int depth) {
    if (depth > kMaxDepth) fail("nesting too deep");
    if (pos_ >= s_.size()) fail("unexpected end of input");
    switch (s_[pos_]) {
      case '{':
        return object(depth);
      case '[':
        return array(depth);
      case '"':
        return Json(string());
      case 't':
        if (literal("true")) return Json(true);
        break;
      case 'f':
        if (literal("false")) return Json(false);
        break;
      case 'n':
        if (literal("null")) return Json(nullptr);
        break;
      default:
        if (s_[pos_] == '-' || at_digit()) return Json(number());
    }
    fail("unexpected character");
  }

  Json object(int depth) {
    ++pos_;  // '{'
    Json::Object members;
    skip_ws();
    if (consume('}')) return members;
    while (true) {
      skip_ws();
      if (pos_ >= s_.size() || s_[pos_] != '"') fail("expected a string key");
      std::string key = string();
      skip_ws();
      expect(':');
      skip_ws();
      Json v = value(depth + 1);
      members.emplace_back(std::move(key), std::move(v));
      skip_ws();
      if (consume(',')) continue;
      expect('}');
      return members;
    }
  }

  Json array(int depth) {
    ++pos_;  // '['
    Json::Array items;
    skip_ws();
    if (consume(']')) return items;
    while (true) {
      skip_ws();
      items.push_back(value(depth + 1));
      skip_ws();
      if (consume(',')) continue;
      expect(']');
      return items;
    }
  }

  std::uint32_t hex4() {
    if (pos_ + 4 > s_.size()) fail("truncated \\u escape");
    std::uint32_t v = 0;
    for (int i = 0; i < 4; ++i) {
      const char c = s_[pos_++];
      v <<= 4;
      if (c >= '0' && c <= '9') v |= static_cast<std::uint32_t>(c - '0');
      else if (c >= 'a' && c <= 'f') v |= static_cast<std::uint32_t>(c - 'a' + 10);
      else if (c >= 'A' && c <= 'F') v |= static_cast<std::uint32_t>(c - 'A' + 10);
      else fail("invalid \\u escape");
    }
    return v;
  }

  static void append_utf8(std::string& out, std::uint32_t cp) {
    const auto byte = [&out](std::uint32_t b) { out += static_cast<char>(b); };
    if (cp < 0x80) {
      byte(cp);
    } else if (cp < 0x800) {
      byte(0xC0 | (cp >> 6));
      byte(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
      byte(0xE0 | (cp >> 12));
      byte(0x80 | ((cp >> 6) & 0x3F));
      byte(0x80 | (cp & 0x3F));
    } else {
      byte(0xF0 | (cp >> 18));
      byte(0x80 | ((cp >> 12) & 0x3F));
      byte(0x80 | ((cp >> 6) & 0x3F));
      byte(0x80 | (cp & 0x3F));
    }
  }

  std::string string() {
    ++pos_;  // opening quote
    std::string out;
    while (true) {
      if (pos_ >= s_.size()) fail("unterminated string");
      const char c = s_[pos_++];
      if (c == '"') return out;
      if (static_cast<unsigned char>(c) < 0x20) fail("control character in string");
      if (c != '\\') {
        out += c;
        continue;
      }
      if (pos_ >= s_.size()) fail("unterminated string");
      switch (s_[pos_++]) {
        case '"': out += '"'; break;
        case '\\': out += '\\'; break;
        case '/': out += '/'; break;
        case 'b': out += '\b'; break;
        case 'f': out += '\f'; break;
        case 'n': out += '\n'; break;
        case 'r': out += '\r'; break;
        case 't': out += '\t'; break;
        case 'u': {
          std::uint32_t cp = hex4();
          if (cp >= 0xD800 && cp <= 0xDBFF) {
            if (!consume('\\') || !consume('u')) fail("unpaired surrogate");
            const std::uint32_t low = hex4();
            if (low < 0xDC00 || low > 0xDFFF) fail("invalid surrogate pair");
            cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
          } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
            fail("unpaired surrogate");
          }
          append_utf8(out, cp);
          break;
        }
        default:
          fail("invalid escape");
      }
    }
  }

  double number() {
    const std::size_t start = pos_;
    consume('-');
    if (consume('0')) {
      // a leading zero stands alone
    } else if (at_digit()) {
      while (at_digit()) ++pos_;
    } else {
      fail("invalid number");
    }
    if (consume('.')) {
      if (!at_digit()) fail("invalid number");
      while (at_digit()) ++pos_;
    }
    if (consume('e') || consume('E')) {
      if (!consume('+')) consume('-');
      if (!at_digit()) fail("invalid number");
      while (at_digit()) ++pos_;
    }
    // The grammar is checked above, so strtod ("C" locale) sees a valid token.
    const std::string token(s_.substr(start, pos_ - start));
    return std::strtod(token.c_str(), nullptr);
  }

  std::string_view s_;
  std::size_t pos_ = 0;
};

void append_number(std::string& out, double v) {
  if (!std::isfinite(v)) {
    out += "null";
    return;
  }
  char buf[32];
  if (v == std::floor(v) && std::abs(v) < 1e15) {
    std::snprintf(buf, sizeof buf, "%.0f", v);
  } else {
    std::snprintf(buf, sizeof buf, "%.12g", v);
  }
  out += buf;
}

void append_quoted(std::string& out, std::string_view s) {
  out += '"';
  for (const char c : s) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof buf, "\\u%04x", static_cast<unsigned>(c));
          out += buf;
        } else {
          out += c;
        }
    }
  }
  out += '"';
}

void newline(std::string& out, int indent, int depth) {
  if (indent < 0) return;
  out += '\n';
  out.append(static_cast<std::size_t>(indent) * static_cast<std::size_t>(depth), ' ');
}

}  // namespace

Json Json::parse(std::string_view text) { return Parser(text).document(); }

std::string Json::dump(int indent) const {
  std::string out;
  dump_to(out, indent, 0);
  return out;
}

void Json::dump_to(std::string& out, int indent, int depth) const {
  std::visit(
      [&](const auto& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::nullptr_t>) {
          out += "null";
        } else if constexpr (std::is_same_v<T, bool>) {
          out += v ? "true" : "false";
        } else if constexpr (std::is_same_v<T, double>) {
          append_number(out, v);
        } else if constexpr (std::is_same_v<T, std::string>) {
          append_quoted(out, v);
        } else if constexpr (std::is_same_v<T, Array>) {
          if (v.empty()) {
            out += "[]";
            return;
          }
          out += '[';
          for (std::size_t i = 0; i < v.size(); ++i) {
            if (i > 0) out += ',';
            newline(out, indent, depth + 1);
            v[i].dump_to(out, indent, depth + 1);
          }
          newline(out, indent, depth);
          out += ']';
        } else {
          static_assert(std::is_same_v<T, Object>);
          if (v.empty()) {
            out += "{}";
            return;
          }
          out += '{';
          for (std::size_t i = 0; i < v.size(); ++i) {
            if (i > 0) out += ',';
            newline(out, indent, depth + 1);
            append_quoted(out, v[i].first);
            out += indent < 0 ? ":" : ": ";
            v[i].second.dump_to(out, indent, depth + 1);
          }
          newline(out, indent, depth);
          out += '}';
        }
      },
      value_);
}

const Json* Json::find(std::string_view key) const noexcept {
  const auto* members = std::get_if<Object>(&value_);
  if (!members) return nullptr;
  for (const auto& [k, v] : *members) {
    if (k == key) return &v;
  }
  return nullptr;
}

std::string Json::get_string(std::string_view key, std::string_view fallback) const {
  const Json* v = find(key);
  return v && v->is_string() ? v->as_string() : std::string(fallback);
}

std::optional<double> Json::get_number(std::string_view key) const {
  const Json* v = find(key);
  if (v && v->is_number()) return v->as_number();
  return std::nullopt;
}

}  // namespace picks
