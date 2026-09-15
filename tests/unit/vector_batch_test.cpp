#include "quanta/execution/vector_batch.h"

#include <arrow/api.h>
#include <gtest/gtest.h>

#include "parquet_fixture.h"

namespace quanta {
namespace {
TEST(VectorBatch, SharesRecordBatchSchemaAndArrays) {
  auto source = testing::MakeFixtureBatch(3).ValueOrDie();
  auto result = VectorBatch::Make(source);
  ASSERT_TRUE(result.ok()) << result.status();
  auto batch = *result;
  EXPECT_EQ(batch.row_count(), 3);
  EXPECT_EQ(batch.column_count(), 4);
  EXPECT_EQ(batch.schema().get(), source->schema().get());
  EXPECT_EQ(batch.record_batch().get(), source.get());
  EXPECT_EQ(batch.column(0).ValueOrDie().get(), source->column(0).get());
  EXPECT_EQ(batch.column("name").ValueOrDie().get(), source->column(2).get());
  auto copy = batch;
  source.reset();
  EXPECT_EQ(copy.row_count(), 3);
  EXPECT_EQ(copy.column("id").ValueOrDie()->length(), 3);
}
TEST(VectorBatch, RejectsNullAndInvalidBatch) {
  EXPECT_TRUE(VectorBatch::Make(nullptr).status().IsInvalid());
  auto source = testing::MakeFixtureBatch(3).ValueOrDie();
  auto invalid = arrow::RecordBatch::Make(source->schema(), 4, source->columns());
  EXPECT_FALSE(VectorBatch::Make(invalid).ok());
}
TEST(VectorBatch, ValidatesColumnAccess) {
  const auto batch = VectorBatch::Make(testing::MakeFixtureBatch(0).ValueOrDie()).ValueOrDie();
  EXPECT_EQ(batch.row_count(), 0);
  EXPECT_TRUE(batch.column(-1).status().IsIndexError());
  EXPECT_TRUE(batch.column(4).status().IsIndexError());
  EXPECT_TRUE(batch.column("missing").status().IsKeyError());
}
TEST(VectorBatch, SupportsZeroColumnsAndRejectsAmbiguousNames) {
  auto empty = arrow::RecordBatch::Make(arrow::schema({}), 5, arrow::ArrayVector{});
  auto batch = VectorBatch::Make(empty).ValueOrDie();
  EXPECT_EQ(batch.column_count(), 0);
  EXPECT_EQ(batch.row_count(), 5);
  auto source = testing::MakeFixtureBatch(1).ValueOrDie();
  auto duplicate = arrow::RecordBatch::Make(
      arrow::schema({arrow::field("a", arrow::int64()), arrow::field("a", arrow::int64())}), 1,
      {source->column(0), source->column(0)});
  EXPECT_TRUE(VectorBatch::Make(duplicate).ValueOrDie().column("a").status().IsKeyError());
}
TEST(VectorBatch, PreservesSlicedArrayOffsets) {
  auto source = testing::MakeFixtureBatch(10).ValueOrDie();
  auto batch = VectorBatch::Make(source->Slice(3, 4)).ValueOrDie();
  EXPECT_EQ(batch.row_count(), 4);
  auto ids = std::static_pointer_cast<arrow::Int64Array>(batch.column(0).ValueOrDie());
  EXPECT_EQ(ids->offset(), 3);
  EXPECT_EQ(ids->Value(0), -7);
  EXPECT_EQ(ids->data()->buffers[1].get(), source->column(0)->data()->buffers[1].get());
}
}  // namespace
}  // namespace quanta
