# ADR 003: Integrate a parser in Milestone 3

Status: Proposed; no parser dependency installed.

## Context

Implementing a full SQL grammar would consume effort better spent on binding,
planning and execution. Parsing is a distinct concern from semantic correctness.

## Decision

Evaluate Hyrise SQL Parser as a candidate in Milestone 3. Before selecting a pinned
version, review supported syntax, AST ownership, maintenance, licensing and error
reporting. Adapt its AST into Quanta-owned representations at a clear boundary.
This is a planned candidate, not an implemented integration or final dependency
commitment.

## Consequences

Quanta will own name/type binding, logical and physical plans, optimization and
execution regardless of the parser choice. Unsupported syntax must be rejected
explicitly. The current CLI is not a SQL shell.
