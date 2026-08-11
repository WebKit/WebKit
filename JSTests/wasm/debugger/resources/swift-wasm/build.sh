#!/bin/bash

# This script is used to compile Swift WASM.
# Note that the corresponding tests needs to be updated due to updated WASM binary.
#
# Usage:
#   ./build.sh <test-folder>   # build if folder exists, scaffold a new Swift package otherwise

set -e

# Resolve swift binary: prefer swiftly default location, fall back to PATH.
# Install via Swiftly: https://www.swift.org/documentation/articles/wasm-getting-started.html
if [ -x "$HOME/.swiftly/bin/swift" ]; then
    SWIFT="$HOME/.swiftly/bin/swift"
elif command -v swift &>/dev/null; then
    SWIFT=$(command -v swift)
else
    echo "Error: swift not found. Install via Swiftly: https://www.swift.org/documentation/articles/wasm-getting-started.html"
    exit 1
fi

SDK=swift-6.3.1-RELEASE_wasm

if [ -z "$1" ]; then
    echo "Usage: $0 <test-folder>"
    exit 1
fi

FOLDER="$1"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

if [ -d "$SCRIPT_DIR/$FOLDER" ]; then
    FOLDER_DIR="$SCRIPT_DIR/$FOLDER"

    # Detect whether the folder contains multiple Swift sub-packages (no top-level
    # Package.swift, but sub-directories that each have their own Package.swift).
    if [ ! -f "$FOLDER_DIR/Package.swift" ] && ls "$FOLDER_DIR"/*/Package.swift &>/dev/null; then
        echo "Building Swift WebAssembly $FOLDER (multi-package)..."
        for pkg_dir in "$FOLDER_DIR"/*/; do
            pkg_name="$(basename "$pkg_dir")"
            if [ ! -f "$pkg_dir/Package.swift" ]; then
                continue
            fi
            echo "  Building $pkg_name..."
            cd "$pkg_dir"
            "$SWIFT" build --swift-sdk "$SDK" --configuration release
            mv ".build/wasm32-unknown-wasip1/release/$pkg_name.wasm" "$FOLDER_DIR/$pkg_name.wasm"
            rm -rf .build
            echo "  $pkg_name.wasm ready."
        done
        echo "Done! All .wasm files for $FOLDER are ready for testing."
    else
        # Single-package folder: standard build.
        echo "Building Swift WebAssembly $FOLDER..."
        cd "$FOLDER_DIR"
        "$SWIFT" build --swift-sdk "$SDK" --configuration release

        # Move compiled WASM beside main.js
        echo "Moving $FOLDER.wasm..."
        mv ".build/wasm32-unknown-wasip1/release/$FOLDER.wasm" "./$FOLDER.wasm"

        # Clean up build artifacts
        echo "Cleaning up..."
        rm -rf .build

        echo "Done! $FOLDER.wasm is ready for testing."
    fi
else
    echo "Creating new Swift WASM test folder '$FOLDER'..."
    cd "$SCRIPT_DIR"
    mkdir "$FOLDER"
    cd "$FOLDER"
    "$SWIFT" package init --type executable
    echo "Done! Edit Sources/$FOLDER/main.swift then run: $0 $FOLDER"
fi
