//@ memoryHog!
//@ skip if $addressBits <= 32
// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --experimental-wasm-memory64

load("wasm-module-builder.js");
load("memory64-common.js");

(function TestMaxMem64Size() {
  // print(arguments.callee.name);
  // This test can fail if 16GB of memory cannot be allocated.
  allowOOM(() => BasicMemory64Tests(max_num_pages));
})();
