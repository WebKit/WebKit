//@ requireOptions("--useBBQJIT=1", "--useWasmLLInt=1", "--wasmLLIntTiersUpToBBQ=1")
//@ skip
// gc-js-interop-helpers.js needs to have %function remapping and then we can add it to run-jsc-stress-test loading..

// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

load("gc-js-interop-helpers.js");
export let {struct, array} = CreateWasmObjects();
