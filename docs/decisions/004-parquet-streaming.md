# ADR 004: Stream Parquet RecordBatches

Status: Accepted and implemented for Milestone 1.

## Context

Reading an entire file into an Arrow Table before execution creates an unnecessary
whole-file memory dependency. Future analytical workloads can exceed cache or
available memory, and consumers may stop early.

## Decision

Read footer/schema metadata at open, then lazily obtain a Parquet RecordBatchReader
on the first scan call. Set batch size explicitly and disable Arrow pre-buffering
and threaded decoding. Keep the owning FileReader alive until after the batch
reader is destroyed. Return batches with shared-buffer lifetimes independent of
the scanner.

Resolve requested top-level fields to physical leaf columns and restore requested
output order by rearranging array references. Use explicit EOF and sticky errors.
No predicate pruning is implemented.

## Consequences

Storage does not eagerly materialize a whole file, and early consumer termination
avoids further batch requests. Batch size bounds returned rows, while Parquet
pages, dictionaries and retained outputs contribute additional memory. Streaming
supports incremental processing of larger-than-cache data but does not establish
a tested out-of-memory guarantee. Compressed and malformed-file failures may be
reported during iteration after metadata inspection succeeds.
