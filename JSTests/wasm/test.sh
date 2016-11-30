#!/bin/bash

set -e
set -u
set -x

JSSHELL="${JSSHELL:-jsc}"
find . -name "test_*.js" -type f | sort | \
    xargs -n1 -t -I{} "$JSSHELL" -m {}

echo "All tests passed"
