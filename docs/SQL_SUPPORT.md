# SQL support

**SQL is not implemented in Milestone 1.** The executable accepts storage commands
only. Selecting columns with `scan --columns` does not imply SQL support.

## Intended eventual v1 subset — planned

- SELECT and aliases
- WHERE, boolean expressions, comparisons and arithmetic expressions
- INNER equi-joins
- GROUP BY with COUNT, SUM, AVG, MIN and MAX
- ORDER BY and LIMIT
- EXPLAIN and EXPLAIN ANALYZE

Exact type coercion, overflow, null, name-resolution and ordering semantics must
be specified and tested when those features are implemented. The list is a scope
proposal, not a compatibility guarantee.

## Outside v1

Transactions; INSERT, UPDATE and DELETE; subqueries; window functions; arbitrary
join types; a network database server; distributed query execution.
