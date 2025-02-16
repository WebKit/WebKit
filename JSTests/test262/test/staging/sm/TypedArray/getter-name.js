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
var BUGNUMBER = 1180290;
var summary = 'TypedArray getters should have get prefix';

print(BUGNUMBER + ": " + summary);

let TypedArray = Object.getPrototypeOf(Float32Array.prototype).constructor;

assert.sameValue(Object.getOwnPropertyDescriptor(TypedArray, Symbol.species).get.name, "get [Symbol.species]");
assert.sameValue(Object.getOwnPropertyDescriptor(TypedArray.prototype, "buffer").get.name, "get buffer");
assert.sameValue(Object.getOwnPropertyDescriptor(TypedArray.prototype, "byteLength").get.name, "get byteLength");
assert.sameValue(Object.getOwnPropertyDescriptor(TypedArray.prototype, "byteOffset").get.name, "get byteOffset");
assert.sameValue(Object.getOwnPropertyDescriptor(TypedArray.prototype, "length").get.name, "get length");
assert.sameValue(Object.getOwnPropertyDescriptor(TypedArray.prototype, Symbol.toStringTag).get.name, "get [Symbol.toStringTag]");

