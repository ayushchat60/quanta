# Architecture

## Implemented in Milestone 1

The current runtime consists of `quanta_storage`, an internal CLI library, and
the `quanta` executable. The build exposes `Quanta::storage` for CMake consumers
inside the source build. There is no installed SDK/package export yet.

- `include/quanta/execution/vector_batch.h`: execution-facing batch view.
- `include/quanta/storage/parquet_scanner.h`: storage options, metadata and scan API.
- `include/quanta/common/parse_integer.h`: strict integer parsing shared by tools.
- `src/`: implementation, separate from public headers.
- `app/`: parsing and bounded diagnostic display; no SQL frontend.
- `tests/fixtures/`: deterministic Parquet writing used only by test/benchmark tooling.

Future subsystem directories are omitted until they contain implemented code.

### VectorBatch

`VectorBatch::Make(shared_ptr<RecordBatch>)` rejects null and structurally invalid
batches. Arrow's inexpensive `Validate` checks structure; `ValidateFull` is used
in tests for deeper value validation. Construction shares the original batch,
schema, arrays and buffers. Copying a VectorBatch copies shared ownership, not
values. Batch data remains alive after a scanner or another batch is destroyed.

The wrapper offers row/column counts, schema access and checked indexed/named
column access. Named access rejects missing and ambiguous names. The underlying
RecordBatch is exposed as `shared_ptr<const RecordBatch>` for interop.

Immutability is logical rather than deep: Arrow arrays and schemas expose shared
objects, and an external alias could mutate a buffer. Callers must not do so.
Move-from objects follow normal C++ rules; use them only for assignment or
destruction. Future selection vectors can be attached as row-index views without
changing the underlying arrays, but no selection behavior is present today.

### ParquetScanner

`Open(path, options)` creates an exclusively owned, forward-only scanner and reads
footer/schema metadata. It does not read a whole Arrow Table. Metadata includes:

- caller-supplied path (identity label, not a canonical path or immutable file ID),
- Arrow schema and logical top-level column count,
- physical Parquet leaf-column count,
- total rows and a row-group vector with row counts and compressed/uncompressed
  column data sizes. These are not total file sizes including footer/headers.

Typed min/max statistics and predicate decisions are deferred. The row-group
representation provides a place to add statistics later without coupling future
pruning rules to the decoder implementation.

The first `Next()` lazily creates Arrow's RecordBatchReader. The scanner owns both
readers; its member order destroys the batch reader before the file reader it
borrows. Returned VectorBatches retain their own shared data. Dropping the scanner
early closes its reader resources through RAII. Retaining batches retains memory.

Arrow threaded decoding and pre-buffering are disabled. The scanner requests all
row groups in file order, streams decoded batches, and does not concatenate a
whole file. It skips empty Arrow batches. Each returned batch has at most the
requested row count; batches can span row groups or be smaller at boundaries.
Consumers must not assume each batch is a row group or that all batches are full.

`Next()` returns `Result<optional<VectorBatch>>`: a value is data, `nullopt` is EOF,
and an error is failure. EOF remains EOF on subsequent calls. Read errors are
sticky: later calls return the same error rather than resume from an uncertain
position. To rescan, open another scanner. Files must not change during a scan;
there is no snapshot or locking mechanism.

### Projection

An absent `ScanOptions::columns` requests all fields. Explicit empty lists and
duplicate requests are errors. Names resolve to exactly one whole top-level Arrow
field; ambiguous or unknown names produce a KeyError. Matching is exact and
case-sensitive. Dotted paths are not interpreted as nested-child projections.

Parquet stores leaf columns, so a nested top-level field can map to multiple
physical columns. The scanner walks Arrow's schema manifest to collect all those
leaf indices. It then maps the reader's output schema back to requested order and
uses `RecordBatch::SelectColumns`, which rearranges array references without
copying values. Unrequested leaf columns are not decoded. This is storage
projection, not a SQL projection operator.

### Errors

Fallible APIs return Arrow `Result`/`Status`, preserving status codes and adding
operation and path context at storage boundaries. Option/name errors also include
the problematic value. Parquet exceptions are translated at `Open` and `Next`.
Allocation failures and unexpected standard-library exceptions are not universally
translated; the CLI catches `std::exception` at its outer process boundary.
There is no process termination for ordinary library errors. Tests use
`ValueOrDie` only where test setup or an assertion expects success.

CLI exit codes are 0 for success, 2 for command syntax errors, and 1 for storage
or output failures. Library-level invalid projection options are storage errors
at the CLI boundary. Partial display can precede a late read error; check exit
status. The CLI materializes individual scalars only for limited display, not in
the storage path.

## Planned architecture

```text
SQL
 ↓
Parser → Binder / semantic analysis → Logical plan → Rule-based optimizer
 ↓ (optimized logical plan)
Physical planner → Physical operators → VectorBatch → Arrow / Parquet
```

Parsing, binding, planning, optimization and execution remain distinct stages.
Catalog and common utilities support these stages without circular dependencies.
Quanta will implement expression evaluation, selection vectors, filters,
projections, aggregation, joins, pruning decisions, profiling and parallel
execution. None of these future execution stages is implemented in Milestone 1.

Arrow and Parquet provide arrays, schemas, buffers, builders, file encoding and
decoding, and metadata. Arrow Compute and Acero are not used to implement engine
operators. DuckDB may later serve external correctness and benchmark tooling; it
will not execute Quanta runtime queries.

## Memory and concurrency limits

Batch size bounds output rows, not total bytes or resident memory. Large strings,
Parquet pages, dictionaries and caller-retained batches can be large. No measured
peak-memory bound is claimed. Scanner instances are independent but each instance
requires a single consumer; there is no concurrent query scheduler. Future
concurrency should prefer thread-local or partitioned state with explicit merges.
