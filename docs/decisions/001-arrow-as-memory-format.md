# ADR 001: Arrow as the columnar memory representation

Status: Accepted and implemented for Milestone 1.

## Context

Quanta needs typed contiguous column data, null bitmaps, schemas and buffer
ownership. Reimplementing all of these would delay work on the query engine and
create unnecessary interoperability and memory-safety risk.

## Decision

Use Arrow arrays and RecordBatches behind a small Quanta VectorBatch view. Preserve
shared-buffer ownership and avoid deep copies when wrapping, copying or reordering
batches. Treat shared buffers as immutable by contract; const handles alone do not
prevent mutation through other aliases. Parquet decoding still allocates buffers:
zero-copy wrapping does not mean zero-copy file decoding.

## Consequences

Arrow is a runtime dependency and Arrow types/Result appear in the initial public
API. VectorBatch localizes future execution-facing behavior but is not a promise
that Arrow could be replaced without API changes. Quanta still owns expressions,
operators, joins, aggregation, planning, optimization and scheduling. Arrow Compute
and Acero do not implement those systems.
