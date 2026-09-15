#pragma once

#include <iosfwd>
#include <span>
#include <string_view>

namespace quanta::cli {
int Run(std::span<const std::string_view> args, std::ostream& output, std::ostream& error);
}
