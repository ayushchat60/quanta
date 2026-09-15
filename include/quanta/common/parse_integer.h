#pragma once

#include <arrow/result.h>

#include <charconv>
#include <cstdint>
#include <string_view>
#include <system_error>

namespace quanta {
inline arrow::Result<std::int64_t> ParseNonnegativeInteger(std::string_view text,
                                                           std::string_view option) {
  if (text.empty()) return arrow::Status::Invalid(option, " requires a nonempty integer");
  std::int64_t value = 0;
  const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
  if (error != std::errc{} || end != text.data() + text.size() || value < 0) {
    return arrow::Status::Invalid(option, " requires a nonnegative 64-bit integer, got '", text,
                                  "'");
  }
  return value;
}
}  // namespace quanta
