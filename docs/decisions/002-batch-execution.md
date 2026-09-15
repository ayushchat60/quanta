# ADR 002: Batches as the execution-facing unit

Status: Accepted. Batch storage interface implemented; expression execution planned.

## Context

Analytical workloads touch many values in a few columns. Per-row virtual dispatch
and scattered row representations can impede locality and add dispatch overhead.
Actual improvements must be measured on specified workloads.

## Decision

Expose streams of VectorBatch objects with a configurable maximum row count.
Keep arrays columnar and return shared ownership. Future operators will consume
batches and may attach selection vectors to identify logical rows without copying
filtered values. No selection, filtering or expression behavior is implemented yet.

## Consequences

Consumers must handle empty input, final partial batches and arbitrary boundaries.
The row count of a batch is not a memory-size guarantee, especially for strings or
nested values. Batch size is a measurement dimension, not a universally optimal
constant. Batch orientation alone does not imply SIMD instructions or a measured
speedup.
