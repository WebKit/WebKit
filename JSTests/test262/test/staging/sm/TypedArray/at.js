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
    assert.sameValue(constructor.prototype.at.length, 1);

    assert.sameValue(new constructor([0]).at(0), 0);
    assert.sameValue(new constructor([0]).at(-1), 0);

    assert.sameValue(new constructor([]).at(0), undefined);
    assert.sameValue(new constructor([]).at(-1), undefined);
    assert.sameValue(new constructor([]).at(1), undefined);

    assert.sameValue(new constructor([0, 1]).at(0), 0);
    assert.sameValue(new constructor([0, 1]).at(1), 1);
    assert.sameValue(new constructor([0, 1]).at(-2), 0);
    assert.sameValue(new constructor([0, 1]).at(-1), 1);

    assert.sameValue(new constructor([0, 1]).at(2), undefined);
    assert.sameValue(new constructor([0, 1]).at(-3), undefined);
    assert.sameValue(new constructor([0, 1]).at(-4), undefined);
    assert.sameValue(new constructor([0, 1]).at(Infinity), undefined);
    assert.sameValue(new constructor([0, 1]).at(-Infinity), undefined);
    assert.sameValue(new constructor([0, 1]).at(NaN), 0); // ToInteger(NaN) = 0

    // Called from other globals.
    if (typeof createNewGlobal === "function") {
        var at = createNewGlobal()[constructor.name].prototype.at;
        assert.sameValue(at.call(new constructor([1, 2, 3]), 2), 3);
    }

    // Throws if `this` isn't a TypedArray.
    var invalidReceivers = [undefined, null, 1, false, "", Symbol(), [], {}, /./,
                            new Proxy(new constructor(), {})];
    invalidReceivers.forEach(invalidReceiver => {
        assertThrowsInstanceOf(() => {
            constructor.prototype.at.call(invalidReceiver);
        }, TypeError, "Assert that 'at' fails if this value is not a TypedArray");
    });

    // Test that the length getter is never called.
    assert.sameValue(Object.defineProperty(new constructor([1, 2, 3]), "length", {
        get() {
            throw new Error("length accessor called");
        }
    }).at(1), 2);
}

