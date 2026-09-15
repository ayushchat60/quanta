# Roadmap

Milestones describe acceptance gates, not dates. Only Milestone 1 is implemented.
Later milestones remain proposals and must preserve independent correctness checks.

## 1. Foundation + Columnar Parquet Storage Core — implemented

- **Objective:** establish a defensible build and a usable streaming storage boundary.
- **Functionality:** C++20/CMake, Arrow/Parquet discovery, VectorBatch, metadata,
  ordered column scans, configurable batches, storage CLI, generated fixtures,
  tests, benchmarks, sanitizer options, documentation and Ubuntu CI configuration.
- **Design concerns:** shared-buffer lifetimes, nested leaf mapping, bounded batch
  interface, contextual errors, safe destruction order and honest capability claims.
- **Tests:** deterministic values/nulls, empty and one-row input, multiple row
  groups, batch boundaries, malformed input, late read failure, projection and CLI.
- **Acceptance:** local Debug/Release and sanitizer tests pass; benchmark and CLI
  smoke runs succeed. Hosted Ubuntu workflow execution remains to be verified.

## 2. Vector Expressions + Selection Vectors — planned

- **Objective:** evaluate a small, typed expression set over batches in Quanta.
- **Functionality:** explicit selection-vector representation, column/literal
  references, comparisons, arithmetic and boolean expression evaluation.
- **Design concerns:** nullable values, three-valued boolean logic, integer
  overflow, selected-row indexing, stable array ownership and no Arrow Compute.
- **Tests:** scalar reference comparisons, null truth tables, zero/all selected
  rows, invalid types, extreme values and partial-batch boundaries.
- **Acceptance:** documented expression semantics and deterministic selected-row
  results; no SQL parser or query planning required for this milestone.

## 3. SQL Parser + Binder + Logical Plans — planned

- **Objective:** translate a limited SQL subset into typed logical plans.
- **Functionality:** parser adapter, catalog lookup, aliases, binding and plan nodes.
- **Design concerns:** parser AST ownership, error locations, ambiguous names,
  unsupported syntax and clear stage boundaries. Hyrise SQL Parser is a candidate.
- **Tests:** accepted/rejected syntax, binding/type errors and logical plan shape.
- **Acceptance:** supported SQL binds to a documented plan with useful diagnostics.

## 4. Physical Planning + Scan / Filter / Project / Limit — planned

- **Objective:** execute simple supported queries through Quanta operators.
- **Functionality:** physical planner, batch-consuming scan/filter/project/limit.
- **Design concerns:** operator state, early termination, null semantics and lifetimes.
- **Tests:** end-to-end queries, empty input, zero/all matches, limit boundaries,
  column ordering and multiple batches.
- **Acceptance:** supported query results match an independent reference.

## 5. Hash Aggregation — planned

- **Objective:** implement grouped and global analytical aggregation.
- **Functionality:** GROUP BY and COUNT/SUM/AVG/MIN/MAX for specified types.
- **Design concerns:** null keys, empty-input semantics, accumulator overflow,
  key equality, memory usage and deterministic result comparison.
- **Tests:** duplicates, all-null groups, high cardinality, extremes and batch splits.
- **Acceptance:** aggregate results agree with a reference across edge cases.

## 6. Hash Join — planned

- **Objective:** support INNER equi-joins owned by Quanta.
- **Functionality:** build/probe hash join with tested output schema and multiplicity.
- **Design concerns:** duplicate keys, null semantics, key representation and bounded
  output batching; no unsupported arbitrary join types.
- **Tests:** zero matches, repeated keys, asymmetric inputs and cross-batch matches.
- **Acceptance:** row multiplicities and values match an independent reference.

## 7. Rule-Based Optimizer + Projection Pruning + Predicate Pushdown + Parquet Row-Group Pruning — planned

- **Objective:** reduce unnecessary work without changing query results.
- **Functionality:** explicit rewrite rules, column requirements, safe predicate
  placement and metadata-based row-group skipping decisions.
- **Design concerns:** nullable statistics, missing or truncated bounds, unsupported
  types and a way to disable optimization for correctness comparisons.
- **Tests:** optimized/unoptimized equivalence and conservative pruning cases.
- **Acceptance:** identical results, explainable rules and measured skipped work.

## 8. Multithreaded Execution — planned

- **Objective:** safely parallelize appropriate analytical work.
- **Functionality:** partitioned scans and thread-local states with merging.
- **Design concerns:** scheduling, exception propagation, cancellation, ownership,
  deterministic semantics and avoiding per-row global locks.
- **Tests:** varied thread counts, repeated runs, sanitizer checks and sequential
  equivalence; floating-point comparison policy must be specified.
- **Acceptance:** race-free supported paths with reproducible correctness results.

## 9. Row-Oriented Baseline + Reproducible Performance Benchmark Framework — planned

- **Objective:** make execution tradeoffs measurable.
- **Functionality:** independently understandable row baseline, datasets, automated
  runs and machine/compiler/configuration capture.
- **Design concerns:** equal semantics and workloads, cache state, variance and
  setup-time accounting. No invented or incomparable benchmark results.
- **Tests:** correctness before timing, harness validation and result reproducibility.
- **Acceptance:** reproducible Release measurements with complete provenance.

## 10. EXPLAIN ANALYZE + DuckDB Differential Testing + TPC-H-Derived Workloads + Release Polish — planned

- **Objective:** make the implemented engine observable and independently verifiable.
- **Functionality:** EXPLAIN/EXPLAIN ANALYZE, operator metrics, external DuckDB oracle
  and a supported TPC-H-derived workload subset.
- **Design concerns:** accurate accounting, profiling overhead, unsupported SQL,
  reproducible datasets and keeping DuckDB outside runtime execution.
- **Tests:** result equivalence, metric consistency and release build workflows.
- **Acceptance:** documented supported workloads and reproducible correctness and
  profiling evidence; no claim of full TPC-H compliance without actual coverage.
