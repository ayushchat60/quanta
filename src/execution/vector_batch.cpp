#include "quanta/execution/vector_batch.h"

#include <arrow/type.h>

#include <utility>

namespace quanta {

arrow::Result<VectorBatch> VectorBatch::Make(std::shared_ptr<arrow::RecordBatch> batch) {
  if (!batch) return arrow::Status::Invalid("VectorBatch requires a non-null RecordBatch");
  ARROW_RETURN_NOT_OK(batch->Validate());
  return VectorBatch(std::move(batch));
}

VectorBatch::VectorBatch(std::shared_ptr<const arrow::RecordBatch> batch)
    : batch_(std::move(batch)) {}
std::int64_t VectorBatch::row_count() const { return batch_->num_rows(); }
int VectorBatch::column_count() const { return batch_->num_columns(); }
const std::shared_ptr<arrow::Schema>& VectorBatch::schema() const { return batch_->schema(); }
const std::shared_ptr<const arrow::RecordBatch>& VectorBatch::record_batch() const {
  return batch_;
}

arrow::Result<std::shared_ptr<arrow::Array>> VectorBatch::column(int index) const {
  if (index < 0 || index >= column_count()) {
    return arrow::Status::IndexError("Column index ", index, " outside [0, ", column_count(), ")");
  }
  return batch_->column(index);
}

arrow::Result<std::shared_ptr<arrow::Array>> VectorBatch::column(const std::string& name) const {
  const auto indices = schema()->GetAllFieldIndices(name);
  if (indices.size() != 1) {
    return arrow::Status::KeyError("Column '", name, "' is missing or ambiguous");
  }
  return column(indices.front());
}

}  // namespace quanta
