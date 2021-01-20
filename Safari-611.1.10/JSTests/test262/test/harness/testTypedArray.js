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

assert.sameValue(typedArrayConstructors[0], Float64Array);
assert.sameValue(typedArrayConstructors[1], Float32Array);
assert.sameValue(typedArrayConstructors[2], Int32Array);
assert.sameValue(typedArrayConstructors[3], Int16Array);
assert.sameValue(typedArrayConstructors[4], Int8Array);
assert.sameValue(typedArrayConstructors[5], Uint32Array);
assert.sameValue(typedArrayConstructors[6], Uint16Array);
assert.sameValue(typedArrayConstructors[7], Uint8Array);
assert.sameValue(typedArrayConstructors[8], Uint8ClampedArray);

assert.sameValue(typedArrayConstructors[0], TAConstructors[0]);
assert.sameValue(typedArrayConstructors[1], TAConstructors[1]);
assert.sameValue(typedArrayConstructors[2], TAConstructors[2]);
assert.sameValue(typedArrayConstructors[3], TAConstructors[3]);
assert.sameValue(typedArrayConstructors[4], TAConstructors[4]);
assert.sameValue(typedArrayConstructors[5], TAConstructors[5]);
assert.sameValue(typedArrayConstructors[6], TAConstructors[6]);
assert.sameValue(typedArrayConstructors[7], TAConstructors[7]);
assert.sameValue(typedArrayConstructors[8], TAConstructors[8]);
