// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking

load("./resources/v8-mjsunit.js", "caller relative");

let called = false;
let cleanup = function(holdings) {
  // See which target we're cleaning up and unregister the other one.
  if (holdings == 1) {
    let success = fg.unregister(key2);
    assertTrue(success || called);
  } else {
    assertSame(holdings, 2);
    let success = fg.unregister(key1);
    assertTrue(success || called);
  }
  called = true;
}

let fg = new FinalizationRegistry(cleanup);
let key1 = Symbol();
let key2 = Symbol();
// Create two symbols and register them in the FinalizationRegistry. The symbols
// need to be inside a closure so that we can reliably kill them!

(function() {
    for (let i = 0; i < 1000; ++i) {
        let symbol1 = Symbol();
        fg.register(symbol1, 1, key1);
        let symbol2 = Symbol();
        fg.register(symbol2, 2, key2);
    }
})();

// This GC will reclaim target symbols and schedule cleanup.
gc();
assertFalse(called);

// Assert that the cleanup function was called and cleaned up one holdings (but not the other one).
let timeout_func = function() {
    assertTrue(called);
}

setTimeout(timeout_func, 0);
