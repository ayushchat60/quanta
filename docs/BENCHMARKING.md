# Benchmarking

## Implemented benchmark

`quanta_scan_benchmark` uses Google Benchmark 1.9.5 to measure real scanner work.
It creates a deterministic temporary Parquet file before timing and removes it
on exit. The fixture contains 65,536 rows, four columns (`id`, `value`, `name`,
`flag`), patterned nulls, and eight row groups of 8,192 rows. Compression is
UNCOMPRESSED; Parquet's normal writer encoding/dictionary defaults apply.

Six cases scan either all columns or only `id`, with batch sizes 1,024, 4,096 and
16,384. Each timed iteration opens the file, reads metadata, streams every batch,
checks the total row count, and destroys the scanner. Fixture construction is
excluded. The benchmark reports wall time and rows processed; it is single-threaded.
It decodes arrays but does not perform SQL, expressions, printing or aggregation.

Repeated scans normally benefit from the operating system's file cache. This is
not a cold-disk benchmark. A short smoke run is an infrastructure check, not a
stable baseline or a comparison with another engine.

```sh
scripts/build_release.sh -DQUANTA_BUILD_BENCHMARKS=ON
build/release/quanta_scan_benchmark --benchmark_min_time=0.01s
```

For an actual measurement session, after recording the environment and checking
correctness, use longer runs and repetitions:

```sh
mkdir -p benchmarks/results
build/release/quanta_scan_benchmark \
  --benchmark_min_time=1s --benchmark_repetitions=10 \
  --benchmark_out=benchmarks/results/local.json --benchmark_out_format=json
```

Generated results are ignored by Git. Deliberately curated published results
should include their provenance and methodology. Do not publish machine-local
paths or environment secrets embedded in raw output.

On Apple Silicon, Google Benchmark can report unavailable CPU frequency and
thread-affinity metadata. Such output is not a trustworthy machine specification;
record the actual processor model separately. Do not infer frequency or capability
from its fallback `MHz` display.

## Measurement requirements

Performance claims require real, repeatable measurements. Use Release builds for
all comparisons; do not compare sanitizer or Debug timings to optimized builds.
Record at least:

- revision, compiler/version, optimization flags, build type and dependency versions;
- machine model, CPU, core counts, memory, operating system and storage;
- dataset schema, row count, byte size, generation recipe and row-group layout;
- projection, batch size, null distribution and any other workload parameters;
- thread count, cache/warmup policy, repetitions, elapsed-time method and setup costs;
- machine load, power/thermal conditions where relevant, and variability;
- the exact command and independently checked expected results.

Report medians/distributions as appropriate, not only a best case. Record whether
I/O, file opening, allocation and result consumption are included. Compare engines
only on equal semantics and comparable configurations. Resume performance claims
must be reproducible from recorded artifacts. Never invent benchmark numbers.

## Planned dimensions (not implemented benchmarks)

Row-at-a-time versus vectorized execution; scan throughput; filter selectivity;
column projection; group cardinality; hash joins; batch size; row-group pruning;
optimizer enabled/disabled; 1/2/4/8-thread scaling; TPC-H-derived workloads.

Add each dimension only after the corresponding feature and independent
correctness checks exist. CI compiles benchmarks but does not gate correctness on
noisy performance thresholds. No Quanta-versus-engine speed claim exists today.
