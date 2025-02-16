// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-TypedArray-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
for (var constructor of anyTypedArrayConstructors) {
    if (typeof createNewGlobal !== 'function')
        break;

    // G[constructor.name].from, where G is any global, produces an array whose prototype
    // is G[constructor.name].prototype.
    var g = createNewGlobal();
    var ga = g[constructor.name].from([1, 2, 3]);
    assert.sameValue(ga instanceof g[constructor.name], true);

    // %TypedArray%.from can be applied to a constructor from another realm.
    var p = constructor.from.call(g[constructor.name], [1, 2, 3]);
    assert.sameValue(p instanceof g[constructor.name], true);
    var q = g[constructor.name].from.call(constructor, [3, 4, 5]);
    assert.sameValue(q instanceof constructor, true);

    // The default 'this' value received by a non-strict mapping function is
    // that function's global, not %TypedArray%.from's global or the caller's global.
    var h = createNewGlobal(), result = undefined;
    h.mainGlobal = this;
    h.eval("function f() { mainGlobal.result = this; }");
    g[constructor.name].from.call(constructor, [5, 6, 7], h.f);
    // (Give each global in the test a name, for better error messages.  But use
    // globalName, because window.name is complicated.)
    this.globalName = "main";
    g.globalName = "g";
    h.globalName = "h";
    assert.sameValue(result.globalName, "h");
}

