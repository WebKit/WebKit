// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js, deepEqual.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
let Array_unscopables = Array.prototype[Symbol.unscopables];

let desc = Reflect.getOwnPropertyDescriptor(Array.prototype, Symbol.unscopables);
assert.deepEqual(desc, {
    value: Array_unscopables,
    writable: false,
    enumerable: false,
    configurable: true
});

assert.sameValue(Reflect.getPrototypeOf(Array_unscopables), null);

let desc2 = Object.getOwnPropertyDescriptor(Array_unscopables, "values");
assert.deepEqual(desc2, {
    value: true,
    writable: true,
    enumerable: true,
    configurable: true
});

let keys = Reflect.ownKeys(Array_unscopables);

// FIXME: Once bug 1826643 is fixed, change this test so that all
// the keys are in alphabetical order
let expectedKeys = ["at",
		    "copyWithin",
		    "entries",
		    "fill",
		    "find",
		    "findIndex",
		    "findLast",
		    "findLastIndex",
		    "flat",
		    "flatMap",
		    "includes",
		    "keys",
            "toReversed",
            "toSorted",
            "toSpliced",
		    "values"];

assert.deepEqual(keys, expectedKeys);

for (let key of keys)
    assert.sameValue(Array_unscopables[key], true);

// Test that it actually works
assertThrowsInstanceOf(() => {
    with ([]) {
        return entries;
    }
}, ReferenceError);

{
    let fill = 33;
    with (Array.prototype) {
        assert.sameValue(fill, 33);
    }
}

