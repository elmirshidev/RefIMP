# RefIMP (formative draft, 5 November 2026)

RefIMP is a small, C-like imperative language derived from IMP and extended with
lexical scopes, heap-allocated integer references, and a proposed static type system.

## Current status

- **Implemented:** AST-based interpreter; integer/Boolean values, local variable
  declarations and mutation, arithmetic/comparisons, scoped blocks, if/while,
  `new(e)`, `*e`, and `*e := e`.
- **Implemented:** five demonstration programs *as constructed ASTs* in C++:
  addition, half, factorial, Fibonacci, and aliasing.
- **Planned:** lexer and parser (to directly execute the `.refimp` example files),
  static type checker, stronger runtime diagnostics, and complete test suite.
- **Not supported:** first-class functions, arbitrary reference types, manual
  deallocation, garbage collection, or compiling to machine code.

The `.refimp` examples define the planned **concrete source syntax**; this draft
interpreter **does not yet parse those files**. Do not claim otherwise in the report.

## Building and running the working AST prototype

Requirements: C++17 compiler, e.g. `g++`.

```bash
g++ -std=c++17 -Wall -Wextra -pedantic -O2 src/main.cpp -o refimp-ast
./refimp-ast add 3 4            # 7
./refimp-ast half 7             # 3
./refimp-ast factorial 5       # 120
./refimp-ast fibonacci 8       # 21
./refimp-ast alias 5 7         # 12
./test.sh
```

The prototype accepts natural-number command-line inputs and prints one integer.
It dispatches by demo name because parsing is **still future work**.

## Semantics and design choices

- `Int`, `Bool`, `Ref Int` are the proposed static types.
- `new(e)` evaluates `e` and allocates a fresh heap cell containing its integer result.
- `*e` evaluates `e` to an address and retrieves its integer content.
- `*e1 := e2` writes to a heap cell; references may alias.
- Binary expressions and writes evaluate their subexpressions left to right.
- Blocks introduce lexical scopes; assigning a visible outer binding persists.
- Heap cells persist for the full program (no deallocation).
- Inputs must be natural numbers. Internal signed `int64_t` arithmetic may overflow;
  practical use should stay within representable values. A future implementation
  should explicitly validate arithmetic overflow.

## Report

See `report/RefIMP-draft.pdf`. 

## Academic integrity 

This formative scaffold was prepared with AI assistance based on the assignment
brief and the proposed language design.
