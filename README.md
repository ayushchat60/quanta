# Quanta

Quanta is a C++20 single-node analytical query engine project. It is designed to
make database internals understandable while holding the implementation to
production-quality engineering standards.

## Current status: Milestone 1 — storage foundation

Implemented and tested locally on macOS with Homebrew LLVM and Arrow/Parquet:

- A small `VectorBatch` abstraction sharing Arrow RecordBatch buffers.
- Parquet schema, file counts, and per-row-group row counts and byte sizes.
- Forward-only batch streaming with configurable batch sizes.
- All-column scans and ordered projections of named top-level fields, including
  nested structs. Requested fields resolve to physical Parquet leaf columns.
- Null preservation, partial batches, clean EOF, and contextual errors.
- A storage CLI, generated test fixtures, unit/integration tests, sanitizer
  options, and real scan benchmark infrastructure.

SQL, expressions, filtering, joins, aggregation, optimization, and parallel query
execution are future work. Quanta does not yet execute SQL queries.

## Build

Requires CMake 3.24+, a C++20 compiler, and installed shared Arrow and Parquet
CMake packages (23+; locally verified with 25.0.1). GoogleTest 1.18.0 and optional
Google Benchmark 1.9.5 are downloaded as pinned, SHA-256-checked releases.

### macOS

```sh
scripts/bootstrap_macos.sh
CMAKE_GENERATOR=Ninja scripts/build_debug.sh
scripts/test.sh
```

The bootstrap installs missing Homebrew dependencies. Build scripts locate LLVM
with `brew --prefix llvm`; `CXX` can override the compiler. They do not change
shell configuration. Ninja is optional.

Equivalent explicit configuration:

```sh
cmake -S . -B build/debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER="$(brew --prefix llvm)/bin/clang++" \
  -DCMAKE_PREFIX_PATH="$(brew --prefix apache-arrow)"
cmake --build build/debug --parallel 4
ctest --test-dir build/debug --output-on-failure
```

### Ubuntu 24.04

```sh
scripts/bootstrap_ubuntu.sh
CXX=clang++-18 CMAKE_GENERATOR=Ninja scripts/build_debug.sh
scripts/test.sh
```

The bootstrap uses sudo to install development packages and configure the
[official Apache Arrow APT source](https://arrow.apache.org/install/).
The GitHub Actions workflow targets Ubuntu 24.04; Linux execution has not been
verified locally on the macOS development machine.

### Build options

| Option | Default | Purpose |
| --- | --- | --- |
| `QUANTA_BUILD_TESTS` | ON | GoogleTest tests and fixture generator |
| `QUANTA_BUILD_BENCHMARKS` | OFF | Google Benchmark scanner benchmarks and fixture generator |
| `QUANTA_ENABLE_ASAN` | OFF | AddressSanitizer for Quanta code |
| `QUANTA_ENABLE_UBSAN` | OFF | UndefinedBehaviorSanitizer for Quanta code |

`QUANTA_BUILD_DIR` and `QUANTA_JOBS` override script build location and parallel
build jobs. Use separate build directories for different compilers and modes.
To build just the runtime, disable both tests and benchmarks.

## Try the CLI

```sh
build/debug/quanta version
build/debug/quanta_make_fixture build/example.parquet 23 7
build/debug/quanta inspect build/example.parquet
build/debug/quanta scan build/example.parquet
build/debug/quanta scan build/example.parquet --columns name,id --batch-size 4 --limit 5
```

The fixture tool generates deterministic values in integer, double, string and
boolean columns, including nulls; it refuses to overwrite an existing file.
It is available when tests or benchmarks are enabled.

`scan` defaults to displaying 20 rows in batches of at most 4096 rows. `--limit 0`
prints the header without reading batches. The display limit stops further
scanning, though the last fetched batch may contain additional rows. The summary
reports displayed rows and fetched batches, not a full-file scan count. Strings
are quoted and control characters escaped; nulls print as `NULL`. Output is for
inspection, not a general serialization format. Column names containing commas
can be used through the C++ API but cannot be expressed in the CLI column list.

## Architecture

Current path:

```text
CLI / C++ caller → ParquetScanner → VectorBatch → shared Arrow arrays
                        ↓
                  Parquet file reader
```

Arrow owns columnar memory representation and Parquet decoding. Quanta owns the
storage boundary and will own expressions, operators, planning, and optimization.
The runtime uses neither Arrow Compute/Acero nor DuckDB for query execution.
See [architecture](docs/ARCHITECTURE.md) and [design decisions](docs/decisions/).

## Development and verification

```sh
scripts/format.sh --check
QUANTA_BUILD_DIR=build/asan scripts/build_debug.sh -DQUANTA_ENABLE_ASAN=ON
scripts/test.sh build/asan
QUANTA_BUILD_DIR=build/ubsan scripts/build_debug.sh -DQUANTA_ENABLE_UBSAN=ON
scripts/test.sh build/ubsan
scripts/build_release.sh -DQUANTA_BUILD_BENCHMARKS=ON
build/release/quanta_scan_benchmark --benchmark_min_time=0.01s
```

CI uses clang-format 18; `CLANG_FORMAT` overrides the formatter. The configuration
also works with the locally installed clang-format 23. `.clang-tidy` enables a
small correctness-focused check set; a compile database is generated in each
build directory. Tests create and clean their own temporary files. Sanitizers
instrument Quanta, not the installed Arrow/Parquet binaries.

Benchmark smoke runs verify infrastructure and provide no comparative performance
claim. See [benchmark methodology](docs/BENCHMARKING.md).

## Scope and limitations

Local files only, one consumer per scanner, no file snapshot isolation, no rewind,
no cancellation, no predicate pruning, and no parallel scanning. Treat input
files and shared array buffers as immutable while scanning. Batch size limits
rows, not bytes: decoder pages, dictionaries and retained batches still consume
memory. Nested struct projection is tested; not every Parquet logical type,
encoding, codec, encryption mode or malformed-file case is verified.

See the [roadmap](docs/ROADMAP.md) and [SQL scope](docs/SQL_SUPPORT.md).

## License

[MIT](LICENSE).
