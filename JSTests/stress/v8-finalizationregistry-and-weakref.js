//@ requireOptions("--useWeakRefs=1")

// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking

load("./resources/v8-mjsunit.js", "caller relative");

let cleanup_called = false;
let cleanup = function(holdings) {
  let holdings_list = [];
  holdings_list.push(holdings);
  assertEquals(1, holdings_list.length);
  assertEquals("holdings", holdings_list[0]);
  cleanup_called = true;
}

let fg = new FinalizationRegistry(cleanup);
let weak_refs = [];
(function() {
    for (let i = 0; i < 1000; ++i) {
        let o = {};
        weak_refs.push(new WeakRef(o));
        fg.register(o, "holdings");
    }
})();

// Since the WeakRef was created during this turn, it is not cleared by GC. The
// pointer inside the FinalizationRegistry is not cleared either, since the WeakRef
// keeps the target object alive.
gc();
(function() {
    weak_refs.forEach((weak_ref) => assertEquals("object", typeof(weak_ref.deref())));
})();

// Trigger gc in next task
setTimeout(() => {
    gc();

    // Check that cleanup callback was called in a follow up task
    setTimeout(() => {
        assertTrue(cleanup_called);
        assertTrue(weak_refs.some((weak_ref) => weak_ref.deref() === null));
    }, 0);
}, 0);
