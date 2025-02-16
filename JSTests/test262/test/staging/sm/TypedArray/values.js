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
    assert.sameValue(constructor.prototype.values.length, 0);
    assert.sameValue(constructor.prototype.values.name, "values");
    assert.sameValue(constructor.prototype.values, constructor.prototype[Symbol.iterator]);

    assert.deepEqual([...new constructor(0).values()], []);
    assert.deepEqual([...new constructor(1).values()], [0]);
    assert.deepEqual([...new constructor(2).values()], [0, 0]);
    assert.deepEqual([...new constructor([15]).values()], [15]);

    var arr = new constructor([1, 2, 3]);
    var iterator = arr.values();
    assert.deepEqual(iterator.next(), {value: 1, done: false});
    assert.deepEqual(iterator.next(), {value: 2, done: false});
    assert.deepEqual(iterator.next(), {value: 3, done: false});
    assert.deepEqual(iterator.next(), {value: undefined, done: true});

    // Called from other globals.
    if (typeof createNewGlobal === "function") {
        var values = createNewGlobal()[constructor.name].prototype.values;
        assert.deepEqual([...values.call(new constructor([42, 36]))], [42, 36]);
        arr = new (createNewGlobal()[constructor.name])([42, 36]);
        assert.sameValue([...constructor.prototype.values.call(arr)].toString(), "42,36");
    }

    // Throws if `this` isn't a TypedArray.
    var invalidReceivers = [undefined, null, 1, false, "", Symbol(), [], {}, /./,
                            new Proxy(new constructor(), {})];
    invalidReceivers.forEach(invalidReceiver => {
        assertThrowsInstanceOf(() => {
            constructor.prototype.values.call(invalidReceiver);
        }, TypeError, "Assert that values fails if this value is not a TypedArray");
    });
}

