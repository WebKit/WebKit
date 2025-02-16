// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-TypedArray-shell.js, deepEqual.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
for (var constructor of anyTypedArrayConstructors) {
    assert.deepEqual(constructor.prototype.reverse.length, 0);

    assert.deepEqual(new constructor().reverse(), new constructor());
    assert.deepEqual(new constructor(10).reverse(), new constructor(10));
    assert.deepEqual(new constructor([]).reverse(), new constructor([]));
    assert.deepEqual(new constructor([1]).reverse(), new constructor([1]));
    assert.deepEqual(new constructor([1, 2]).reverse(), new constructor([2, 1]));
    assert.deepEqual(new constructor([1, 2, 3]).reverse(), new constructor([3, 2, 1]));
    assert.deepEqual(new constructor([1, 2, 3, 4]).reverse(), new constructor([4, 3, 2, 1]));
    assert.deepEqual(new constructor([1, 2, 3, 4, 5]).reverse(), new constructor([5, 4, 3, 2, 1]));
    assert.deepEqual(new constructor([.1, .2, .3]).reverse(), new constructor([.3, .2, .1]));

    // Called from other globals.
    if (typeof createNewGlobal === "function") {
        var reverse = createNewGlobal()[constructor.name].prototype.reverse;
        assert.deepEqual(reverse.call(new constructor([3, 2, 1])), new constructor([1, 2, 3]));
    }

    // Throws if `this` isn't a TypedArray.
    var invalidReceivers = [undefined, null, 1, false, "", Symbol(), [], {}, /./,
                            new Proxy(new constructor(), {})];
    invalidReceivers.forEach(invalidReceiver => {
        assertThrowsInstanceOf(() => {
            constructor.prototype.reverse.call(invalidReceiver);
        }, TypeError, "Assert that reverse fails if this value is not a TypedArray");
    });

    // Test that the length getter is never called.
    Object.defineProperty(new constructor([1, 2, 3]), "length", {
        get() {
            throw new Error("length accessor called");
        }
    }).reverse();
}

