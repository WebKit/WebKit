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

var callCount = 0;
testWithTypedArrayConstructors(() => callCount++);
assert.sameValue(callCount, 9);

assert.sameValue(typedArrayConstructors[0], Float64Array);
assert.sameValue(typedArrayConstructors[1], Float32Array);
assert.sameValue(typedArrayConstructors[2], Int32Array);
assert.sameValue(typedArrayConstructors[3], Int16Array);
assert.sameValue(typedArrayConstructors[4], Int8Array);
assert.sameValue(typedArrayConstructors[5], Uint32Array);
assert.sameValue(typedArrayConstructors[6], Uint16Array);
assert.sameValue(typedArrayConstructors[7], Uint8Array);
assert.sameValue(typedArrayConstructors[8], Uint8ClampedArray);
