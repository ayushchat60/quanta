#include "quanta/storage/parquet_scanner.h"

#include <parquet/arrow/reader.h>
#include <parquet/arrow/schema.h>
#include <parquet/exception.h>
#include <parquet/file_reader.h>
#include <parquet/metadata.h>
#include <parquet/properties.h>

#include <numeric>
#include <unordered_set>
#include <utility>

namespace quanta {
namespace {
arrow::Status Context(const arrow::Status& status, const std::string& operation,
                      const std::string& path) {
  return status.ok() ? status : status.WithMessage(operation, " '", path, "': ", status.message());
}

void CollectLeaves(const parquet::arrow::SchemaField& field, std::vector<int>& indices) {
  if (field.is_leaf()) indices.push_back(field.column_index);
  for (const auto& child : field.children) CollectLeaves(child, indices);
}
}  // namespace

ParquetScanner::ParquetScanner() = default;
ParquetScanner::~ParquetScanner() = default;

arrow::Result<std::unique_ptr<ParquetScanner>> ParquetScanner::Open(const std::string& path,
                                                                    ScanOptions options) {
  auto scanner = std::unique_ptr<ParquetScanner>(new ParquetScanner());
  try {
    ARROW_RETURN_NOT_OK(Context(scanner->Initialize(path, options), "Opening Parquet", path));
  } catch (const parquet::ParquetException& error) {
    return arrow::Status::IOError("Opening Parquet '", path, "': ", error.what());
  }
  return scanner;
}

arrow::Status ParquetScanner::Initialize(const std::string& path, const ScanOptions& options) {
  if (options.batch_size <= 0) return arrow::Status::Invalid("batch_size must be positive");
  if (options.columns && options.columns->empty()) {
    return arrow::Status::Invalid("Explicit projection must contain at least one column");
  }
  parquet::arrow::FileReaderBuilder builder;
  ARROW_RETURN_NOT_OK(builder.OpenFile(path));
  parquet::ArrowReaderProperties properties;
  properties.set_batch_size(options.batch_size);
  properties.set_use_threads(false);
  // Avoid asynchronous whole-row-group prefetch; decoding still uses Parquet page buffers.
  properties.set_pre_buffer(false);
  builder.properties(properties);
  ARROW_ASSIGN_OR_RAISE(file_reader_, builder.Build());
  metadata_.path = path;
  ARROW_RETURN_NOT_OK(file_reader_->GetSchema(&metadata_.schema));
  const auto parquet_metadata = file_reader_->parquet_reader()->metadata();
  metadata_.row_count = parquet_metadata->num_rows();
  metadata_.column_count = metadata_.schema->num_fields();
  metadata_.leaf_column_count = parquet_metadata->num_columns();
  for (int i = 0; i < parquet_metadata->num_row_groups(); ++i) {
    const auto group = parquet_metadata->RowGroup(i);
    metadata_.row_groups.push_back(
        {group->num_rows(), group->total_compressed_size(), group->total_byte_size()});
  }
  columns_ = options.columns;
  if (columns_) {
    std::unordered_set<std::string> seen;
    for (const auto& name : *columns_) {
      if (!seen.insert(name).second) return arrow::Status::Invalid("Duplicate column '", name, "'");
      const auto indices = metadata_.schema->GetAllFieldIndices(name);
      if (indices.size() != 1) {
        return arrow::Status::KeyError("Requested column '", name, "' is missing or ambiguous");
      }
      CollectLeaves(file_reader_->manifest().schema_fields.at(indices.front()), leaf_indices_);
    }
  }
  return arrow::Status::OK();
}

arrow::Status ParquetScanner::StartStream() {
  std::vector<int> groups(metadata_.row_groups.size());
  std::iota(groups.begin(), groups.end(), 0);
  if (columns_) {
    ARROW_ASSIGN_OR_RAISE(batch_reader_, file_reader_->GetRecordBatchReader(groups, leaf_indices_));
    for (const auto& name : *columns_) {
      const int index = batch_reader_->schema()->GetFieldIndex(name);
      if (index < 0) return arrow::Status::Invalid("Projected field missing from reader: ", name);
      output_indices_.push_back(index);
    }
  } else {
    ARROW_ASSIGN_OR_RAISE(batch_reader_, file_reader_->GetRecordBatchReader(groups));
  }
  return arrow::Status::OK();
}

arrow::Result<std::optional<VectorBatch>> ParquetScanner::ReadNext() {
  if (exhausted_) return std::nullopt;
  if (!batch_reader_) ARROW_RETURN_NOT_OK(StartStream());
  std::shared_ptr<arrow::RecordBatch> batch;
  // Empty batches are valid Arrow output, but do not represent useful scan progress.
  do {
    ARROW_RETURN_NOT_OK(batch_reader_->ReadNext(&batch));
  } while (batch && batch->num_rows() == 0);
  if (!batch) {
    exhausted_ = true;
    return std::nullopt;
  }
  if (columns_) {
    ARROW_ASSIGN_OR_RAISE(batch, batch->SelectColumns(output_indices_));
  }
  ARROW_ASSIGN_OR_RAISE(auto result, VectorBatch::Make(std::move(batch)));
  return result;
}

arrow::Result<std::optional<VectorBatch>> ParquetScanner::Next() {
  if (!failure_.ok()) return failure_;
  try {
    auto result = ReadNext();
    if (!result.ok()) failure_ = Context(result.status(), "Reading Parquet", metadata_.path);
    return failure_.ok() ? std::move(result) : failure_;
  } catch (const parquet::ParquetException& error) {
    failure_ = arrow::Status::IOError("Reading Parquet '", metadata_.path, "': ", error.what());
    return failure_;
  }
}

}  // namespace quanta
