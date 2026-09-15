#pragma once

#include <arrow/record_batch.h>
#include <arrow/result.h>

#include <cstdint>
#include <memory>
#include <string>

namespace quanta {

// A logically immutable, shared view of columnar data. Aliases must not mutate buffers.
class VectorBatch {
 public:
  static arrow::Result<VectorBatch> Make(std::shared_ptr<arrow::RecordBatch> batch);

  [[nodiscard]] std::int64_t row_count() const;
  [[nodiscard]] int column_count() const;
  [[nodiscard]] const std::shared_ptr<arrow::Schema>& schema() const;
  [[nodiscard]] arrow::Result<std::shared_ptr<arrow::Array>> column(int index) const;
  [[nodiscard]] arrow::Result<std::shared_ptr<arrow::Array>> column(const std::string& name) const;
  [[nodiscard]] const std::shared_ptr<const arrow::RecordBatch>& record_batch() const;

 private:
  explicit VectorBatch(std::shared_ptr<const arrow::RecordBatch> batch);
  std::shared_ptr<const arrow::RecordBatch> batch_;
};

}  // namespace quanta
