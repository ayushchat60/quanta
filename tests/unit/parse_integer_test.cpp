#include "quanta/common/parse_integer.h"

#include <gtest/gtest.h>

#include <limits>

namespace quanta {
namespace {
TEST(ParseInteger, AcceptsZeroAndMaximum) {
  EXPECT_EQ(ParseNonnegativeInteger("0", "count").ValueOrDie(), 0);
  EXPECT_EQ(ParseNonnegativeInteger("9223372036854775807", "count").ValueOrDie(),
            std::numeric_limits<std::int64_t>::max());
}
TEST(ParseInteger, RejectsEmptyOverflowAndMalformedValuesWithContext) {
  for (const auto value :
       {std::string_view{}, std::string_view("9223372036854775808"), std::string_view("-1"),
        std::string_view(" 1"), std::string_view("+1"), std::string_view("1x")}) {
    auto result = ParseNonnegativeInteger(value, "count");
    EXPECT_TRUE(result.status().IsInvalid());
    EXPECT_NE(result.status().ToString().find("count"), std::string::npos);
  }
}
}  // namespace
}  // namespace quanta
