#!/bin/bash

# This is a barebones script that checks all the files exposed in compile_commands.json for errors
# under clangd.
# It's only been tested under Linux, and it's provided here mostly for reference purposes.

DIR="$(dirname "$0")"
cat "$DIR/../../compile_commands.json" | jq .[].file -r | xargs -P32 -n1 "$DIR/clangd-check-file"