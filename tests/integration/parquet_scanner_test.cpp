#include "quanta/storage/parquet_scanner.h"

#include <arrow/api.h>
#include <gtest/gtest.h>

#include <fstream>
#include <limits>

#include "parquet_fixture.h"

namespace quanta {
namespace {
class ParquetScanTest : public ::testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(testing::WriteFixture(temp.file()).ok()); }
  testing::TempDirectory temp;
};

TEST_F(ParquetScanTest, ReadsMetadataWithoutScanning) {
  auto result = ParquetScanner::Open(temp.file().string());
  ASSERT_TRUE(result.ok()) << result.status();
  const auto& metadata = (*result)->metadata();
  EXPECT_EQ(metadata.path, temp.file().string());
  EXPECT_EQ(metadata.row_count, 23);
  EXPECT_EQ(metadata.column_count, 4);
  EXPECT_EQ(metadata.leaf_column_count, 4);
  ASSERT_EQ(metadata.row_groups.size(), 4);
  EXPECT_EQ(metadata.row_groups[0].row_count, 7);
  EXPECT_EQ(metadata.row_groups[3].row_count, 2);
  for (const auto& group : metadata.row_groups) {
    EXPECT_GT(group.compressed_bytes, 0);
    EXPECT_GT(group.uncompressed_bytes, 0);
  }
  EXPECT_TRUE(metadata.schema->Equals(*testing::MakeFixtureBatch(0).ValueOrDie()->schema()));
}

class BatchSizeTest : public ParquetScanTest, public ::testing::WithParamInterface<int> {};
TEST_P(BatchSizeTest, StreamsEveryValueAcrossRowGroupsAndBatchBoundaries) {
  const int size = GetParam();
  auto scanner =
      ParquetScanner::Open(temp.file().string(), {.columns = std::nullopt, .batch_size = size})
          .ValueOrDie();
  std::int64_t row = 0;
  int batches = 0;
  std::int64_t final_size = 0;
  while (true) {
    auto result = scanner->Next();
    ASSERT_TRUE(result.ok()) << result.status();
    if (!*result) break;
    const auto& batch = **result;
    ++batches;
    final_size = batch.row_count();
    ASSERT_GT(batch.row_count(), 0);
    EXPECT_LE(batch.row_count(), size);
    ASSERT_EQ(batch.column_count(), 4);
    ASSERT_TRUE(batch.record_batch()->ValidateFull().ok());
    const auto ids = std::static_pointer_cast<arrow::Int64Array>(batch.column(0).ValueOrDie());
    const auto values = std::static_pointer_cast<arrow::DoubleArray>(batch.column(1).ValueOrDie());
    const auto names = std::static_pointer_cast<arrow::StringArray>(batch.column(2).ValueOrDie());
    const auto flags = std::static_pointer_cast<arrow::BooleanArray>(batch.column(3).ValueOrDie());
    for (std::int64_t i = 0; i < batch.row_count(); ++i, ++row) {
      EXPECT_EQ(ids->Value(i), row - 10);
      EXPECT_EQ(values->IsNull(i), row % 5 == 0);
      if (!values->IsNull(i))
        EXPECT_DOUBLE_EQ(values->Value(i), static_cast<double>(row) * 0.5 - 3.0);
      EXPECT_EQ(names->IsNull(i), row % 7 == 0);
      if (!names->IsNull(i))
        EXPECT_EQ(names->GetString(i), row % 3 == 0 ? "" : "row-" + std::to_string(row));
      EXPECT_EQ(flags->IsNull(i), row % 4 == 0);
      if (!flags->IsNull(i)) EXPECT_EQ(flags->Value(i), row % 2 == 0);
    }
  }
  EXPECT_EQ(row, 23);
  EXPECT_GT(batches, 0);
  if (size < 23) EXPECT_GT(batches, 1);
  if (size == 4) EXPECT_LT(final_size, size);
  EXPECT_FALSE(scanner->Next().ValueOrDie().has_value());
  EXPECT_FALSE(scanner->Next().ValueOrDie().has_value());
}
INSTANTIATE_TEST_SUITE_P(Boundaries, BatchSizeTest, ::testing::Values(1, 4, 7, 8, 23, 64));

TEST_F(ParquetScanTest, ProjectionPreservesRequestedOrderAndValues) {
  auto scanner =
      ParquetScanner::Open(temp.file().string(), {.columns = {{"name", "id"}}, .batch_size = 4})
          .ValueOrDie();
  std::int64_t row = 0;
  while (auto batch = scanner->Next().ValueOrDie()) {
    ASSERT_EQ(batch->column_count(), 2);
    EXPECT_EQ(batch->schema()->field(0)->name(), "name");
    EXPECT_EQ(batch->schema()->field(1)->name(), "id");
    const auto names = std::static_pointer_cast<arrow::StringArray>(batch->column(0).ValueOrDie());
    const auto ids = std::static_pointer_cast<arrow::Int64Array>(batch->column(1).ValueOrDie());
    for (std::int64_t i = 0; i < batch->row_count(); ++i, ++row) {
      EXPECT_EQ(ids->Value(i), row - 10);
      EXPECT_EQ(names->IsNull(i), row % 7 == 0);
      if (!names->IsNull(i))
        EXPECT_EQ(names->GetString(i), row % 3 == 0 ? "" : "row-" + std::to_string(row));
    }
  }
  EXPECT_EQ(row, 23);
}
TEST_F(ParquetScanTest, RejectsInvalidOptionsWithContext) {
  const auto path = temp.file().string();
  for (const auto& columns : std::vector<std::vector<std::string>>{{}, {"id", "id"}, {"unknown"}}) {
    const auto result = ParquetScanner::Open(path, {.columns = columns});
    EXPECT_FALSE(result.ok());
    EXPECT_NE(result.status().ToString().find(path), std::string::npos);
  }
  const auto unknown = ParquetScanner::Open(path, {.columns = {{"unknown"}}});
  EXPECT_TRUE(unknown.status().IsKeyError());
  EXPECT_NE(unknown.status().ToString().find("unknown"), std::string::npos);
  for (const int size : {0, -1}) {
    EXPECT_TRUE(ParquetScanner::Open(path, {.columns = std::nullopt, .batch_size = size})
                    .status()
                    .IsInvalid());
  }
}
TEST_F(ParquetScanTest, BatchesOutliveScannerAndLaterReads) {
  auto scanner = ParquetScanner::Open(temp.file().string(), {.columns = {{"id"}}, .batch_size = 4})
                     .ValueOrDie();
  const auto first = scanner->Next().ValueOrDie();
  while (scanner->Next().ValueOrDie()) {
  }
  scanner.reset();
  ASSERT_TRUE(first.has_value());
  auto ids = std::static_pointer_cast<arrow::Int64Array>(first->column(0).ValueOrDie());
  EXPECT_EQ(ids->Value(0), -10);
  EXPECT_EQ(ids->Value(3), -7);
}
TEST_F(ParquetScanTest, IndependentScansHaveIndependentPosition) {
  auto first = ParquetScanner::Open(temp.file().string()).ValueOrDie();
  auto second = ParquetScanner::Open(temp.file().string()).ValueOrDie();
  auto a = first->Next().ValueOrDie();
  auto b = second->Next().ValueOrDie();
  ASSERT_TRUE(a && b);
  EXPECT_TRUE(a->record_batch()->Equals(*b->record_batch()));
}
TEST_F(ParquetScanTest, MissingAndCorruptFilesReturnContextualErrors) {
  const auto missing = temp.file("missing.parquet").string();
  const auto result = ParquetScanner::Open(missing);
  EXPECT_TRUE(result.status().IsIOError());
  EXPECT_NE(result.status().ToString().find(missing), std::string::npos);
  std::ofstream(temp.file("bad.parquet")) << "not parquet";
  const auto corrupt = ParquetScanner::Open(temp.file("bad.parquet").string());
  EXPECT_FALSE(corrupt.ok());
  EXPECT_NE(corrupt.status().ToString().find("bad.parquet"), std::string::npos);
  std::filesystem::copy_file(temp.file(), temp.file("truncated.parquet"));
  std::filesystem::resize_file(temp.file("truncated.parquet"), 20);
  EXPECT_FALSE(ParquetScanner::Open(temp.file("truncated.parquet").string()).ok());
}
TEST_F(ParquetScanTest, CorruptDataPageFailsLazilyAndErrorIsSticky) {
  // Preserve the footer/schema and magic, corrupt only the first data page header.
  std::fstream file(temp.file(), std::ios::in | std::ios::out | std::ios::binary);
  file.seekp(4);
  const std::string invalid_header(16, static_cast<char>(0xff));
  file.write(invalid_header.data(), static_cast<std::streamsize>(invalid_header.size()));
  file.close();
  auto opened = ParquetScanner::Open(temp.file().string());
  ASSERT_TRUE(opened.ok()) << opened.status();
  EXPECT_EQ((*opened)->metadata().row_count, 23);
  auto first = (*opened)->Next();
  ASSERT_FALSE(first.ok());
  EXPECT_NE(first.status().ToString().find(temp.file().string()), std::string::npos);
  EXPECT_NE(first.status().ToString().find("Reading Parquet"), std::string::npos);
  EXPECT_EQ((*opened)->Next().status().ToString(), first.status().ToString());
  // Projecting an intact column must avoid decoding the corrupted first column.
  auto projected = ParquetScanner::Open(temp.file().string(), {.columns = {{"flag"}}}).ValueOrDie();
  std::int64_t rows = 0;
  while (auto batch = projected->Next().ValueOrDie()) rows += batch->row_count();
  EXPECT_EQ(rows, 23);
}

TEST_F(ParquetScanTest, EmptyAndOneRowFiles) {
  for (const int rows : {0, 1}) {
    ASSERT_TRUE(testing::WriteFixture(temp.file("small.parquet"), rows).ok());
    auto scanner =
        ParquetScanner::Open(temp.file("small.parquet").string(), {.columns = {{"flag", "id"}}})
            .ValueOrDie();
    EXPECT_EQ(scanner->metadata().row_count, rows);
    auto batch = scanner->Next().ValueOrDie();
    EXPECT_EQ(batch.has_value(), rows != 0);
    if (batch) {
      EXPECT_EQ(batch->row_count(), 1);
      EXPECT_EQ(batch->column_count(), 2);
      EXPECT_TRUE(batch->column(0).ValueOrDie()->IsNull(0));
    }
    EXPECT_FALSE(scanner->Next().ValueOrDie());
  }
}
TEST_F(ParquetScanTest, NestedProjectionUsesLeafIndicesAndTopLevelOrder) {
  auto base = testing::MakeFixtureBatch(23).ValueOrDie();
  auto nested = arrow::StructArray::Make(
                    {base->column(0), base->column(2)},
                    {arrow::field("number", arrow::int64()), arrow::field("text", arrow::utf8())})
                    .ValueOrDie();
  auto batch = arrow::RecordBatch::Make(
      arrow::schema({arrow::field("nested", nested->type()), arrow::field("flag", arrow::boolean()),
                     arrow::field("value", arrow::float64())}),
      23, {nested, base->column(3), base->column(1)});
  ASSERT_TRUE(testing::WriteBatch(temp.file("nested.parquet"), batch, 7).ok());
  auto scanner = ParquetScanner::Open(temp.file("nested.parquet").string(),
                                      {.columns = {{"value", "nested"}}, .batch_size = 4})
                     .ValueOrDie();
  EXPECT_EQ(scanner->metadata().column_count, 3);
  EXPECT_EQ(scanner->metadata().leaf_column_count, 4);
  std::int64_t row = 0;
  while (auto next = scanner->Next().ValueOrDie()) {
    ASSERT_EQ(next->column_count(), 2);
    EXPECT_EQ(next->schema()->field(0)->name(), "value");
    EXPECT_TRUE(
        next->column(0).ValueOrDie()->Equals(base->column(1)->Slice(row, next->row_count())));
    EXPECT_TRUE(next->column(1).ValueOrDie()->Equals(nested->Slice(row, next->row_count())));
    row += next->row_count();
  }
  EXPECT_EQ(row, 23);
}
TEST_F(ParquetScanTest, DuplicateSchemaNamesAllowAllButRejectNamedProjection) {
  auto base = testing::MakeFixtureBatch(1).ValueOrDie();
  auto batch = arrow::RecordBatch::Make(
      arrow::schema({arrow::field("a", arrow::int64()), arrow::field("a", arrow::int64())}), 1,
      {base->column(0), base->column(0)});
  ASSERT_TRUE(testing::WriteBatch(temp.file("duplicate.parquet"), batch, 1).ok());
  auto scanner = ParquetScanner::Open(temp.file("duplicate.parquet").string()).ValueOrDie();
  ASSERT_EQ(scanner->Next().ValueOrDie()->column_count(), 2);
  EXPECT_TRUE(ParquetScanner::Open(temp.file("duplicate.parquet").string(), {.columns = {{"a"}}})
                  .status()
                  .IsKeyError());
}
TEST_F(ParquetScanTest, ExtremeIntegersAndAllNullColumnRoundTrip) {
  arrow::Int64Builder builder;
  ASSERT_TRUE(builder
                  .AppendValues({std::numeric_limits<std::int64_t>::min(),
                                 std::numeric_limits<std::int64_t>::max(), 0, 0})
                  .ok());
  auto numbers = builder.Finish().ValueOrDie();
  auto nulls = arrow::MakeArrayOfNull(arrow::utf8(), 4).ValueOrDie();
  auto batch = arrow::RecordBatch::Make(
      arrow::schema({arrow::field("n", arrow::int64()), arrow::field("nulls", arrow::utf8())}), 4,
      {numbers, nulls});
  ASSERT_TRUE(testing::WriteBatch(temp.file("edge.parquet"), batch, 2).ok());
  auto scanner = ParquetScanner::Open(temp.file("edge.parquet").string(),
                                      {.columns = std::nullopt, .batch_size = 3})
                     .ValueOrDie();
  std::int64_t row = 0;
  while (auto next = scanner->Next().ValueOrDie()) {
    EXPECT_TRUE(next->column(0).ValueOrDie()->Equals(numbers->Slice(row, next->row_count())));
    EXPECT_EQ(next->column(1).ValueOrDie()->null_count(), next->row_count());
    row += next->row_count();
  }
  EXPECT_EQ(row, 4);
}
}  // namespace
}  // namespace quanta
