//@ requireOptions("--useWeakRefs=1")

// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-weak-refs-with-cleanup-some --expose-gc --noincremental-marking

load("./resources/v8-mjsunit.js", "caller relative");

var FR = new FinalizationRegistry (function (holdings) { globalThis.FRRan = true; });
{
    for (let i = 0; i < 1000; ++i) {
        let obj = {};
        // obj is its own unregister token and becomes unreachable after this
        // block. If the unregister token is held strongly this test will not
        // terminate.
        FR.register(obj, 42, obj);
    }
}

let tryAgainCount = 10;
function tryAgain() {
    gc();
    if (globalThis.FRRan)
        return;
    if (!--tryAgainCount)
        throw new Error();
    setTimeout(tryAgain, 0);
}
tryAgain();
