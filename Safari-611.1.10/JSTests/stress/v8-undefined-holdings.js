//@ requireOptions("--useWeakRefs=1")

// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking

load("./resources/v8-mjsunit.js", "caller relative");

let cleanup_call_count = 0;
let cleanup = function(holdings) {
    assertEquals(holdings, undefined);
    ++cleanup_call_count;
}

let fg = new FinalizationRegistry(cleanup);

// Create an object and register it in the FinalizationRegistry. The object needs to be inside
// a closure so that we can reliably kill them!

(function() {
    for (let i = 0; i < 1000; ++i) {
        let object = {};
        fg.register(object);
    }
})();

// This GC will reclaim the target object and schedule cleanup.
gc();
assertEquals(0, cleanup_call_count);

// Assert that the cleanup function was called.
let timeout_func = function() {
    assertNotEquals(0, cleanup_call_count);
}

setTimeout(timeout_func, 0);
