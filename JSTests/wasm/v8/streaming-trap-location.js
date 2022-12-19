//@ requireOptions("--useBBQJIT=1", "--useWasmLLInt=1", "--wasmLLIntTiersUpToBBQ=1")
//@ skip
// Failure:
// Failure (trap reason):
// expected:
// /Division by zero/
// found:
// "Division by zero (evaluating 'instance.exports.main(value)')"
// Looks like we need to update the exception strings.

// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --wasm-test-streaming --expose-wasm

load("trap-location.js");
