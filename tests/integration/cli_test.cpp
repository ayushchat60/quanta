#include "cli.h"

#include <arrow/api.h>
#include <gtest/gtest.h>

#include <sstream>
#include <string_view>
#include <vector>

#include "parquet_fixture.h"

namespace quanta {
namespace {
class CliTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(testing::WriteFixture(temp.file()).ok());
    path = temp.file().string();
  }
  int Run(std::initializer_list<std::string_view> args) {
    return cli::Run(std::span(args.begin(), args.size()), output, error);
  }
  testing::TempDirectory temp;
  std::string path;
  std::ostringstream output;
  std::ostringstream error;
};
TEST_F(CliTest, VersionAndHelp) {
  EXPECT_EQ(Run({"version"}), 0);
  EXPECT_NE(output.str().find("Quanta 0.1.0"), std::string::npos);
  EXPECT_NE(output.str().find("Arrow "), std::string::npos);
  EXPECT_EQ(Run({"--help"}), 0);
  EXPECT_NE(output.str().find("--batch-size"), std::string::npos);
}
TEST_F(CliTest, Inspect) {
  EXPECT_EQ(Run({"inspect", path}), 0);
  EXPECT_NE(output.str().find("Rows: 23"), std::string::npos);
  EXPECT_NE(output.str().find("Columns: 4"), std::string::npos);
  EXPECT_NE(output.str().find("Row groups: 4"), std::string::npos);
}
TEST_F(CliTest, ProjectionBatchSizeAndLimit) {
  EXPECT_EQ(Run({"scan", path, "--columns", "name,id", "--batch-size", "4", "--limit", "5"}), 0);
  EXPECT_TRUE(output.str().starts_with("name\tid\nNULL\t-10\n\"row-1\"\t-9\n"));
  EXPECT_NE(output.str().find("Displayed 5 rows from 2 batches"), std::string::npos);
  EXPECT_EQ(output.str().find("row-5"), std::string::npos);
  EXPECT_TRUE(error.str().empty());
}
TEST_F(CliTest, DefaultLimit) {
  EXPECT_EQ(Run({"scan", path}), 0);
  EXPECT_NE(output.str().find("Displayed 20 rows"), std::string::npos);
}
TEST_F(CliTest, ZeroLimitReadsNoBatches) {
  EXPECT_EQ(Run({"scan", path, "--limit", "0"}), 0);
  EXPECT_NE(output.str().find("Displayed 0 rows from 0 batches"), std::string::npos);
}
TEST_F(CliTest, LimitBeyondEof) {
  EXPECT_EQ(Run({"scan", path, "--batch-size", "4", "--limit", "100"}), 0);
  EXPECT_NE(output.str().find("Displayed 23 rows"), std::string::npos);
}
TEST_F(CliTest, InvalidSyntax) {
  for (const auto& args : std::vector<std::vector<std::string_view>>{
           {},
           {"sql"},
           {"version", "extra"},
           {"inspect", path, "extra"},
           {"scan", path, "--limit"},
           {"scan", path, "--unknown", "2"},
           {"scan", path, "--batch-size", "0"},
           {"scan", path, "--limit", "-1"},
           {"scan", path, "--limit", "1x"},
           {"scan", path, "--limit", "+1"},
           {"scan", path, "--limit", "999999999999999999999999"},
           {"scan", path, "--limit", "2", "--limit", "3"},
           {"scan", path, "--columns", ""},
           {"scan", path, "--columns", "id,"}}) {
    std::ostringstream out;
    std::ostringstream err;
    EXPECT_EQ(cli::Run(args, out, err), 2);
    EXPECT_FALSE(err.str().empty());
  }
}
TEST_F(CliTest, StorageErrors) {
  EXPECT_EQ(Run({"scan", path, "--columns", "missing"}), 1);
  EXPECT_NE(error.str().find("missing"), std::string::npos);
  EXPECT_NE(error.str().find(path), std::string::npos);
  EXPECT_EQ(Run({"inspect", temp.file("missing").string()}), 1);
}
TEST_F(CliTest, DistinguishesNullAndEscapesStrings) {
  arrow::StringBuilder builder;
  ASSERT_TRUE(builder.Append("NULL").ok());
  ASSERT_TRUE(builder.AppendNull().ok());
  ASSERT_TRUE(builder.Append("").ok());
  ASSERT_TRUE(builder.Append("a\tb\nc\r\"\\").ok());
  auto values = builder.Finish().ValueOrDie();
  auto batch =
      arrow::RecordBatch::Make(arrow::schema({arrow::field("text", arrow::utf8())}), 4, {values});
  ASSERT_TRUE(testing::WriteBatch(temp.file("strings.parquet"), batch, 2).ok());
  EXPECT_EQ(Run({"scan", temp.file("strings.parquet").string()}), 0);
  EXPECT_TRUE(output.str().starts_with("text\n\"NULL\"\nNULL\n\"\"\n\"a\\tb\\nc\\r\\\"\\\\\"\n"));
}
TEST_F(CliTest, OutputFailureReturnsError) {
  output.setstate(std::ios::badbit);
  EXPECT_EQ(Run({"scan", path}), 1);
  EXPECT_EQ(Run({"version"}), 1);
  EXPECT_EQ(Run({"--help"}), 1);
  EXPECT_NE(error.str().find("writing output failed"), std::string::npos);
}
}  // namespace
}  // namespace quanta
