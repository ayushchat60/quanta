#pragma once

#include <arrow/result.h>
#include <arrow/type.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "quanta/execution/vector_batch.h"

namespace parquet::arrow {
class FileReader;
}

namespace quanta {

struct RowGroupMetadata {
  std::int64_t row_count;
  std::int64_t compressed_bytes;
  std::int64_t uncompressed_bytes;
};

struct ParquetMetadata {
  std::string path;
  std::shared_ptr<arrow::Schema> schema;
  std::int64_t row_count;
  int column_count;       // Top-level Arrow fields.
  int leaf_column_count;  // Physical Parquet columns (can differ for nested types).
  std::vector<RowGroupMetadata> row_groups;
};

struct ScanOptions {
  // Absent means all columns. Explicit empty, duplicate, or ambiguous names are errors.
  // Names refer to whole top-level fields, including nested fields.
  std::optional<std::vector<std::string>> columns;
  std::int64_t batch_size = 4096;
};

// One forward-only, single-consumer scan. Not safe for concurrent calls.
class ParquetScanner {
 public:
  static arrow::Result<std::unique_ptr<ParquetScanner>> Open(const std::string& path,
                                                             ScanOptions options = {});
  ~ParquetScanner();
  ParquetScanner(const ParquetScanner&) = delete;
  ParquetScanner& operator=(const ParquetScanner&) = delete;

  [[nodiscard]] const ParquetMetadata& metadata() const { return metadata_; }
  // nullopt is permanent EOF. Read errors are sticky. Returned batches own their buffers.
  arrow::Result<std::optional<VectorBatch>> Next();

 private:
  ParquetScanner();
  arrow::Status Initialize(const std::string& path, const ScanOptions& options);
  arrow::Status StartStream();
  arrow::Result<std::optional<VectorBatch>> ReadNext();

  ParquetMetadata metadata_;
  std::optional<std::vector<std::string>> columns_;
  std::vector<int> leaf_indices_;
  std::vector<int> output_indices_;
  // Reverse destruction order is essential: the batch reader borrows the file reader.
  std::unique_ptr<parquet::arrow::FileReader> file_reader_;
  std::unique_ptr<arrow::RecordBatchReader> batch_reader_;
  bool exhausted_ = false;
  arrow::Status failure_;
};

}  // namespace quanta
