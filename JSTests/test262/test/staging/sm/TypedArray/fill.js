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
    assert.deepEqual(constructor.prototype.fill.length, 1);

    assert.deepEqual(new constructor([]).fill(1), new constructor([]));
    assert.deepEqual(new constructor([1,1,1]).fill(2), new constructor([2,2,2]));
    assert.deepEqual(new constructor([1,1,1]).fill(2, 1), new constructor([1,2,2]));
    assert.deepEqual(new constructor([1,1,1]).fill(2, 1, 2), new constructor([1,2,1]));
    assert.deepEqual(new constructor([1,1,1]).fill(2, -2), new constructor([1,2,2]));
    assert.deepEqual(new constructor([1,1,1]).fill(2, -2, -1), new constructor([1,2,1]));
    assert.deepEqual(new constructor([1,1,1]).fill(2, undefined), new constructor([2,2,2]));
    assert.deepEqual(new constructor([1,1,1]).fill(2, undefined, undefined), new constructor([2,2,2]));
    assert.deepEqual(new constructor([1,1,1]).fill(2, 1, undefined), new constructor([1,2,2]));
    assert.deepEqual(new constructor([1,1,1]).fill(2, undefined, 1), new constructor([2,1,1]));
    assert.deepEqual(new constructor([1,1,1]).fill(2, 2, 1), new constructor([1,1,1]));
    assert.deepEqual(new constructor([1,1,1]).fill(2, -1, 1), new constructor([1,1,1]));
    assert.deepEqual(new constructor([1,1,1]).fill(2, -2, 1), new constructor([1,1,1]));
    assert.deepEqual(new constructor([1,1,1]).fill(2, 1, -2), new constructor([1,1,1]));
    assert.deepEqual(new constructor([1,1,1]).fill(2, 0.1), new constructor([2,2,2]));
    assert.deepEqual(new constructor([1,1,1]).fill(2, 0.9), new constructor([2,2,2]));
    assert.deepEqual(new constructor([1,1,1]).fill(2, 1.1), new constructor([1,2,2]));
    assert.deepEqual(new constructor([1,1,1]).fill(2, 0.1, 0.9), new constructor([1,1,1]));
    assert.deepEqual(new constructor([1,1,1]).fill(2, 0.1, 1.9), new constructor([2,1,1]));
    assert.deepEqual(new constructor([1,1,1]).fill(2, 0.1, 1.9), new constructor([2,1,1]));
    assert.deepEqual(new constructor([1,1,1]).fill(2, -0), new constructor([2,2,2]));
    assert.deepEqual(new constructor([1,1,1]).fill(2, 0, -0), new constructor([1,1,1]));
    assert.deepEqual(new constructor([1,1,1]).fill(2, NaN), new constructor([2,2,2]));
    assert.deepEqual(new constructor([1,1,1]).fill(2, 0, NaN), new constructor([1,1,1]));
    assert.deepEqual(new constructor([1,1,1]).fill(2, false), new constructor([2,2,2]));
    assert.deepEqual(new constructor([1,1,1]).fill(2, true), new constructor([1,2,2]));
    assert.deepEqual(new constructor([1,1,1]).fill(2, "0"), new constructor([2,2,2]));
    assert.deepEqual(new constructor([1,1,1]).fill(2, "1"), new constructor([1,2,2]));
    assert.deepEqual(new constructor([1,1,1]).fill(2, "-2"), new constructor([1,2,2]));
    assert.deepEqual(new constructor([1,1,1]).fill(2, "-2", "-1"), new constructor([1,2,1]));
    assert.deepEqual(new constructor([1,1,1]).fill(2, {valueOf: ()=>1}), new constructor([1,2,2]));
    assert.deepEqual(new constructor([1,1,1]).fill(2, 0, {valueOf: ()=>1}), new constructor([2,1,1]));

    // Called from other globals.
    if (typeof createNewGlobal === "function") {
        var fill = createNewGlobal()[constructor.name].prototype.fill;
        assert.deepEqual(fill.call(new constructor([3, 2, 1]), 2), new constructor([2, 2, 2]));
    }

    // Throws if `this` isn't a TypedArray.
    var invalidReceivers = [undefined, null, 1, false, "", Symbol(), [], {}, /./,
                            new Proxy(new constructor(), {})];
    invalidReceivers.forEach(invalidReceiver => {
        assertThrowsInstanceOf(() => {
            constructor.prototype.fill.call(invalidReceiver, 1);
        }, TypeError);
    });

    // Test that the length getter is never called.
    Object.defineProperty(new constructor([1, 2, 3]), "length", {
        get() {
            throw new Error("length accessor called");
        }
    }).fill(1);
}

for (let constructor of anyTypedArrayConstructors.filter(isFloatConstructor)) {
    assert.deepEqual(new constructor([0, 0]).fill(NaN), new constructor([NaN, NaN]));
}

