//@ requireOptions("--useBBQJIT=1", "--useWasmLLInt=1", "--wasmLLIntTiersUpToBBQ=1")
//@ skip
// Failure:
// Exception: Failure (Error message):
// expected:
// expected 1 elements on the stack for fallthru, found 0 @+94"
// found:
// 0 values, in function at index 10"

// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-test-streaming --expose-wasm --allow-natives-syntax

load("async-compile.js");
