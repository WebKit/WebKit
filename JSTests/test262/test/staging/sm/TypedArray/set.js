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
const TypedArrayPrototype = Object.getPrototypeOf(Int8Array.prototype);

// %TypedArrayPrototype% has an own "set" function property.
assert.sameValue(TypedArrayPrototype.hasOwnProperty("set"), true);
assert.sameValue(typeof TypedArrayPrototype.set, "function");

// The concrete TypedArray prototypes do not have an own "set" property.
assert.sameValue(anyTypedArrayConstructors.every(c => !c.hasOwnProperty("set")), true);

assert.deepEqual(Object.getOwnPropertyDescriptor(TypedArrayPrototype, "set"), {
    value: TypedArrayPrototype.set,
    writable: true,
    enumerable: false,
    configurable: true,
});

assert.sameValue(TypedArrayPrototype.set.name, "set");
assert.sameValue(TypedArrayPrototype.set.length, 1);

