//@ requireOptions("--useBBQJIT=1", "--useWebAssemblyLLInt=1", "--webAssemblyLLIntTiersUpToBBQ=1")
//@ skip
// Failure:
// Exception: Did not throw exception

// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --noexpose-wasm

assertThrows(() => { let x = WebAssembly.compile; });
