//@ requireOptions("--useWeakRefs=1")

// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking
// Flags: --no-stress-flush-bytecode

load("./resources/v8-mjsunit.js", "caller relative");

let cleanup0_call_count = 0;
let cleanup0_0_holdings_count = 0;
let cleanup0_1_holdings_count = 0;

let cleanup1_call_count = 0;
let cleanup1_0_holdings_count = 0;
let cleanup1_1_holdings_count = 0;

let cleanup0 = function(holdings) {
  if (holdings[holdings.length - 1] == "0")
    ++cleanup0_0_holdings_count;
  else
    ++cleanup0_1_holdings_count;
  ++cleanup0_call_count;
}

let cleanup1 = function(holdings) {
  if (holdings[holdings.length - 1] == "0")
    ++cleanup1_0_holdings_count;
  else
    ++cleanup1_1_holdings_count;
  ++cleanup1_call_count;
}

let fg0 = new FinalizationRegistry(cleanup0);
let fg1 = new FinalizationRegistry(cleanup1);

// Register 1 weak reference for each FinalizationRegistry and kill the objects they point to.
(function() {
  for (let i = 0; i < 1000; ++i) {
    // The objects need to be inside a closure so that we can reliably kill them.
    let objects = [];
    objects[0] = {};
    objects[1] = {};

    fg0.register(objects[0], "holdings0-0");
    fg1.register(objects[1], "holdings1-0");

    // Drop the references to the objects.
    objects = [];
  }
})();

// Will schedule both fg0 and fg1 for cleanup.
gc();

// Before the cleanup task has a chance to run, do the same thing again, so both
// FinalizationRegistries are (again) scheduled for cleanup. This has to be a IIFE function
// (so that we can reliably kill the objects) so we cannot use the same function
// as before.
(function() {
  for (let i = 0; i < 1000; ++i) {
    let objects = [];
    objects[0] = {};
    objects[1] = {};
    fg0.register(objects[0], "holdings0-1");
    fg1.register(objects[1], "holdings1-1");
    objects = [];
  }
})();

gc();

let timeout_func = function() {
  assertNotEquals(0, cleanup0_call_count);
  assertNotEquals(0, cleanup0_0_holdings_count);
  assertNotEquals(0, cleanup0_1_holdings_count);
  assertNotEquals(0, cleanup1_call_count);
  assertNotEquals(0, cleanup1_0_holdings_count);
  assertNotEquals(0, cleanup1_0_holdings_count);
}

// Give the cleanup task a chance to run.
setTimeout(timeout_func, 0);
