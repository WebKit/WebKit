// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-TypedArray-shell.js, compareArray.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
for (var constructor of anyTypedArrayConstructors) {
    assert.sameValue(constructor.prototype.slice.length, 2);

    assert.compareArray(new constructor().slice(0), new constructor());
    assert.compareArray(new constructor().slice(0, 4), new constructor());
    assert.compareArray(new constructor(10).slice(0, 2), new constructor(2));

    assert.compareArray(new constructor([1, 2]).slice(1), new constructor([2]));
    assert.compareArray(new constructor([1, 2]).slice(0), new constructor([1, 2]));
    assert.compareArray(new constructor([1, 2, 3]).slice(-1), new constructor([3]));
    assert.compareArray(new constructor([1, 2, 3, 4]).slice(-3, -1), new constructor([2, 3]));
    assert.compareArray(new constructor([.1, .2]).slice(0), new constructor([.1, .2]));

    assert.compareArray(new constructor([1, 2]).slice(-3), new constructor([1, 2]));
    assert.compareArray(new constructor([1, 2]).slice(0, -3), new constructor());
    assert.compareArray(new constructor([1, 2]).slice(4), new constructor());
    assert.compareArray(new constructor([1, 2]).slice(1, 5), new constructor([2]));

    // Called from other globals.
    if (typeof createNewGlobal === "function") {
        var slice = createNewGlobal()[constructor.name].prototype.slice;
        assert.compareArray(slice.call(new constructor([3, 2, 1]), 1),
                      new constructor([2, 1]));
    }

    // Throws if `this` isn't a TypedArray.
    var invalidReceivers = [undefined, null, 1, false, "", Symbol(), [], {}, /./,
                            new Proxy(new constructor(), {})];
    invalidReceivers.forEach(invalidReceiver => {
        assertThrowsInstanceOf(() => {
            constructor.prototype.slice.call(invalidReceiver, 0);
        }, TypeError, "Assert that slice fails if this value is not a TypedArray");
    });

    // Test that the length getter is never called.
    Object.defineProperty(new constructor([1, 2, 3]), "length", {
        get() {
            throw new Error("length accessor called");
        }
    }).slice(2);
}

