// Copyright (c) 2017 Rick Waldron.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: >
    Including testTypedArray.js will expose:

        var typedArrayConstructors = [ array of TypedArray constructors ]
        var TypedArray

        testWithTypedArrayConstructors()
        testTypedArrayConversions()

includes: [testTypedArray.js,arrayContains.js]
features: [TypedArray]
---*/
var TAConstructors = [
  Float64Array,
  Float32Array,
  Int32Array,
  Int16Array,
  Int8Array,
  Uint32Array,
  Uint16Array,
  Uint8Array,
  Uint8ClampedArray
];

var length = TAConstructors.length;

assert(
  arrayContains(typedArrayConstructors, TAConstructors),
  "All TypedArray constructors are accounted for"
);
assert(typeof TypedArray === "function");
assert.sameValue(TypedArray, Object.getPrototypeOf(Uint8Array));


var callCount = 0;
testWithTypedArrayConstructors(() => callCount++);
assert.sameValue(callCount, length);

