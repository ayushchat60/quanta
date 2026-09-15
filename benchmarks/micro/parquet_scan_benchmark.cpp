#include <benchmark/benchmark.h>

#include <cstdint>
#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "parquet_fixture.h"
#include "quanta/storage/parquet_scanner.h"

namespace {
void Scan(benchmark::State& state, const std::string& path, bool projected) {
  quanta::ScanOptions options;
  options.batch_size = state.range(0);
  if (projected) options.columns = std::vector<std::string>{"id"};
  for (auto _ : state) {
    auto opened = quanta::ParquetScanner::Open(path, options);
    if (!opened.ok()) {
      state.SkipWithError(opened.status().ToString());
      return;
    }
    auto scanner = std::move(*opened);
    std::int64_t rows = 0;
    while (true) {
      auto result = scanner->Next();
      if (!result.ok()) {
        state.SkipWithError(result.status().ToString());
        return;
      }
      if (!*result) break;
      rows += (**result).row_count();
      auto* record_batch = (**result).record_batch().get();
      benchmark::DoNotOptimize(record_batch);
    }
    if (rows != 65536) {
      state.SkipWithError("Unexpected row count");
      return;
    }
  }
  state.SetItemsProcessed(state.iterations() * 65536);
}
}  // namespace

int main(int argc, char** argv) {
  benchmark::Initialize(&argc, argv);
  if (benchmark::ReportUnrecognizedArguments(argc, argv)) return 1;
  try {
    quanta::testing::TempDirectory temp;
    const auto status = quanta::testing::WriteFixture(temp.file(), 65536, 8192);
    if (!status.ok()) {
      std::cerr << status << '\n';
      return 1;
    }
    const auto path = temp.file().string();
    benchmark::RegisterBenchmark("ParquetScan/All",
                                 [path](benchmark::State& state) { Scan(state, path, false); })
        ->Arg(1024)
        ->Arg(4096)
        ->Arg(16384)
        ->UseRealTime();
    benchmark::RegisterBenchmark("ParquetScan/Projected",
                                 [path](benchmark::State& state) { Scan(state, path, true); })
        ->Arg(1024)
        ->Arg(4096)
        ->Arg(16384)
        ->UseRealTime();
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
