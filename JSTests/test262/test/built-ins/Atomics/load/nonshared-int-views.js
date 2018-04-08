// Copyright (C) 2017 Mozilla Corporation.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.load
description: >
  Test Atomics.load on non-shared integer TypedArrays
includes: [testTypedArray.js]
features: [Atomics, TypedArray]
---*/

var ab = new ArrayBuffer(16);

var int_views = [Int8Array, Uint8Array, Int16Array, Uint16Array, Int32Array, Uint32Array];

if (typeof BigInt !== "undefined") {
  int_views.push(BigInt64Array);
  int_views.push(BigUint64Array);
}

testWithTypedArrayConstructors(function(View) {
  var view = new View(ab);

  assert.throws(TypeError, (() => Atomics.load(view, 0)));
}, int_views);
