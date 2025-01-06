// Copyright (c) 2017 Rick Waldron.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: >
    Including testTypedArray.js will expose:

        var typedArrayConstructors = [ array of TypedArray constructors ]
        var TypedArray

        testWithTypedArrayConstructors()
        testTypedArrayConversions()

includes: [testTypedArray.js]
features: [TypedArray]
---*/

assert(typeof TypedArray === "function");
assert.sameValue(TypedArray, Object.getPrototypeOf(Uint8Array));

var hasFloat16Array = typeof Float16Array !== 'undefined';

var callCount = 0;
testWithTypedArrayConstructors(() => callCount++);
assert.sameValue(callCount, 9 + hasFloat16Array);

var index = 0;

assert.sameValue(typedArrayConstructors[index++], Float64Array);
assert.sameValue(typedArrayConstructors[index++], Float32Array);
if (hasFloat16Array) {
  assert.sameValue(typedArrayConstructors[index++], Float16Array);
}
assert.sameValue(typedArrayConstructors[index++], Int32Array);
assert.sameValue(typedArrayConstructors[index++], Int16Array);
assert.sameValue(typedArrayConstructors[index++], Int8Array);
assert.sameValue(typedArrayConstructors[index++], Uint32Array);
assert.sameValue(typedArrayConstructors[index++], Uint16Array);
assert.sameValue(typedArrayConstructors[index++], Uint8Array);
assert.sameValue(typedArrayConstructors[index++], Uint8ClampedArray);
