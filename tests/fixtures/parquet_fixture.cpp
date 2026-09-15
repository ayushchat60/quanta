#include "parquet_fixture.h"

#include <arrow/api.h>
#include <arrow/io/api.h>
#include <parquet/arrow/writer.h>

#include <random>
#include <stdexcept>
#include <system_error>

namespace quanta::testing {

TempDirectory::TempDirectory() {
  std::random_device random;
  for (int attempt = 0; attempt < 100; ++attempt) {
    auto candidate = std::filesystem::temp_directory_path() /
                     ("quanta-" + std::to_string(random()) + "-" + std::to_string(random()));
    std::error_code error;
    if (std::filesystem::create_directory(candidate, error)) {
      path_ = std::move(candidate);
      return;
    }
    if (error)
      throw std::filesystem::filesystem_error("Creating fixture directory", candidate, error);
  }
  throw std::runtime_error("Cannot create a unique fixture directory");
}
TempDirectory::~TempDirectory() {
  std::error_code ignored;
  std::filesystem::remove_all(path_, ignored);
}
std::filesystem::path TempDirectory::file(const std::string& name) const { return path_ / name; }

arrow::Result<std::shared_ptr<arrow::RecordBatch>> MakeFixtureBatch(std::int64_t rows) {
  if (rows < 0) return arrow::Status::Invalid("Fixture row count must be nonnegative");
  arrow::Int64Builder ids;
  arrow::DoubleBuilder values;
  arrow::StringBuilder names;
  arrow::BooleanBuilder flags;
  for (std::int64_t i = 0; i < rows; ++i) {
    ARROW_RETURN_NOT_OK(ids.Append(i - 10));
    if (i % 5 == 0) {
      ARROW_RETURN_NOT_OK(values.AppendNull());
    } else {
      ARROW_RETURN_NOT_OK(values.Append(static_cast<double>(i) * 0.5 - 3.0));
    }
    if (i % 7 == 0) {
      ARROW_RETURN_NOT_OK(names.AppendNull());
    } else {
      ARROW_RETURN_NOT_OK(names.Append(i % 3 == 0 ? "" : "row-" + std::to_string(i)));
    }
    if (i % 4 == 0) {
      ARROW_RETURN_NOT_OK(flags.AppendNull());
    } else {
      ARROW_RETURN_NOT_OK(flags.Append(i % 2 == 0));
    }
  }
  ARROW_ASSIGN_OR_RAISE(auto id_array, ids.Finish());
  ARROW_ASSIGN_OR_RAISE(auto value_array, values.Finish());
  ARROW_ASSIGN_OR_RAISE(auto name_array, names.Finish());
  ARROW_ASSIGN_OR_RAISE(auto flag_array, flags.Finish());
  auto schema = arrow::schema(
      {arrow::field("id", arrow::int64(), false), arrow::field("value", arrow::float64()),
       arrow::field("name", arrow::utf8()), arrow::field("flag", arrow::boolean())});
  return arrow::RecordBatch::Make(schema, rows, {id_array, value_array, name_array, flag_array});
}

arrow::Status WriteBatch(const std::filesystem::path& path,
                         const std::shared_ptr<arrow::RecordBatch>& batch,
                         std::int64_t row_group_size) {
  if (row_group_size <= 0) return arrow::Status::Invalid("row_group_size must be positive");
  ARROW_RETURN_NOT_OK(batch->ValidateFull());
  ARROW_ASSIGN_OR_RAISE(auto table, arrow::Table::FromRecordBatches({batch}));
  ARROW_ASSIGN_OR_RAISE(auto output, arrow::io::FileOutputStream::Open(path.string()));
  const auto properties =
      parquet::WriterProperties::Builder().compression(parquet::Compression::UNCOMPRESSED)->build();
  const auto arrow_properties = parquet::ArrowWriterProperties::Builder().store_schema()->build();
  ARROW_RETURN_NOT_OK(parquet::arrow::WriteTable(*table, arrow::default_memory_pool(), output,
                                                 row_group_size, properties, arrow_properties));
  return output->Close();
}

arrow::Status WriteFixture(const std::filesystem::path& path, std::int64_t rows,
                           std::int64_t row_group_size) {
  ARROW_ASSIGN_OR_RAISE(auto batch, MakeFixtureBatch(rows));
  return WriteBatch(path, batch, row_group_size);
}

}  // namespace quanta::testing
