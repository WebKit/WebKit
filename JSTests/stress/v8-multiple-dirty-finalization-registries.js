//@ requireOptions("--useWeakRefs=1")

// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking

load("./resources/v8-mjsunit.js", "caller relative");

let cleanup_call_count1 = 0;
let cleanup_call_count2 = 0;
let cleanup = function(holdings) {
    if (holdings === "holdings1")
        ++cleanup_call_count1;
    else if (holdings === "holdings2")
        ++cleanup_call_count2;
    else
        throw new Error();
}

let fg1 = new FinalizationRegistry(cleanup);
let fg2 = new FinalizationRegistry(cleanup);

// Create two objects and register them in FinalizationRegistries. The objects need
// to be inside a closure so that we can reliably kill them!

(function() {
    for (let i = 0; i < 1000; ++i) {
        let object1 = {};
        fg1.register(object1, "holdings1");

        let object2 = {};
        fg2.register(object2, "holdings2");
    }
    // object1 and object2 go out of scope.
})();

// This GC will discover dirty WeakCells and schedule cleanup.
gc();
assertEquals(0, cleanup_call_count1);
assertEquals(0, cleanup_call_count2);

// Assert that the cleanup function was called.
let timeout_func = function() {
    assertNotEquals(0, cleanup_call_count1);
    assertNotEquals(0, cleanup_call_count2);
}

setTimeout(timeout_func, 0);
