//@ requireOptions("--useBBQJIT=1", "--useWasmLLInt=1", "--wasmLLIntTiersUpToBBQ=1")
//@ skip
// Skipping this test due to the following issues:
// call to db.debugger.enable()
// This uses gc-js-interop-async.js which has %calls that haven't been resolved.


// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-gc --allow-natives-syntax

// The implementation of Promises currently takes a different path (a C++
// runtime function instead of a Torque builtin) when the debugger is
// enabled, so exercise that path in this variant of the test.
d8.debugger.enable();
load("gc-js-interop-async.js");
