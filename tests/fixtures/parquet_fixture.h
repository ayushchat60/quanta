#pragma once

#include <arrow/record_batch.h>
#include <arrow/result.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace quanta::testing {

// Unique temporary directory; only this instance's directory is removed on destruction.
class TempDirectory {
 public:
  TempDirectory();
  ~TempDirectory();
  TempDirectory(const TempDirectory&) = delete;
  TempDirectory& operator=(const TempDirectory&) = delete;
  [[nodiscard]] std::filesystem::path file(const std::string& name = "fixture.parquet") const;

 private:
  std::filesystem::path path_;
};

arrow::Result<std::shared_ptr<arrow::RecordBatch>> MakeFixtureBatch(std::int64_t rows);
arrow::Status WriteBatch(const std::filesystem::path& path,
                         const std::shared_ptr<arrow::RecordBatch>& batch,
                         std::int64_t row_group_size);
arrow::Status WriteFixture(const std::filesystem::path& path, std::int64_t rows = 23,
                           std::int64_t row_group_size = 7);

}  // namespace quanta::testing
