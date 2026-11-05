#!/usr/bin/env bash
set -euo pipefail
if [[ ! -x ./refimp-ast ]]; then
  g++ -std=c++17 -Wall -Wextra -pedantic -O2 src/main.cpp -o refimp-ast
fi
# Explicit tests (no metaprogramming/ambiguous argument parsing).
[[ $(./refimp-ast add 8 11) == 19 ]]
[[ $(./refimp-ast half 0) == 0 ]]
[[ $(./refimp-ast half 7) == 3 ]]
[[ $(./refimp-ast half 12) == 6 ]]
[[ $(./refimp-ast factorial 0) == 1 ]]
[[ $(./refimp-ast factorial 5) == 120 ]]
[[ $(./refimp-ast fibonacci 0) == 0 ]]
[[ $(./refimp-ast fibonacci 1) == 1 ]]
[[ $(./refimp-ast fibonacci 8) == 21 ]]
[[ $(./refimp-ast alias 5 7) == 12 ]]
echo 'All 10 AST interpreter checks passed.'
