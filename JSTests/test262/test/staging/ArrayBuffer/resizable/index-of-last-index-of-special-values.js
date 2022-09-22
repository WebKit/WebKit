// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from IndexOfLastIndexOfSpecialValues test
  in V8's mjsunit test typedarray-resizablearraybuffer.js
features: [resizable-arraybuffer]
flags: [onlyStrict]
---*/

class MyFloat32Array extends Float32Array {
}

const floatCtors = [
  Float32Array,
  Float64Array,
  MyFloat32Array
];

function CreateResizableArrayBuffer(byteLength, maxByteLength) {
  return new ArrayBuffer(byteLength, { maxByteLength: maxByteLength });
}

for (let ctor of floatCtors) {
  const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
  const lengthTracking = new ctor(rab);
  lengthTracking[0] = -Infinity;
  lengthTracking[1] = -Infinity;
  lengthTracking[2] = Infinity;
  lengthTracking[3] = Infinity;
  lengthTracking[4] = NaN;
  lengthTracking[5] = NaN;
  assert.sameValue(lengthTracking.indexOf(-Infinity), 0);
  assert.sameValue(lengthTracking.lastIndexOf(-Infinity), 1);
  assert.sameValue(lengthTracking.indexOf(Infinity), 2);
  assert.sameValue(lengthTracking.lastIndexOf(Infinity), 3);
  // NaN is never found.
  assert.sameValue(lengthTracking.indexOf(NaN), -1);
  assert.sameValue(lengthTracking.lastIndexOf(NaN), -1);
}
