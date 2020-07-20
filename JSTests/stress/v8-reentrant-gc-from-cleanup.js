//@ requireOptions("--useWeakRefs=1")

// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs --expose-gc --noincremental-marking

load("./resources/v8-mjsunit.js", "caller relative");

let called = false;
let reentrant_gc = function(holdings) {
    gc();
    called = true;
}

let fg = new FinalizationRegistry(reentrant_gc);

(function() {
    for (let i = 0; i < 10; ++i)
        fg.register({}, 42);
})();

gc();

setTimeout(function() {
    assertTrue(called);
}, 0);
