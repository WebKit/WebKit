// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-%typedarray%.prototype.toSorted
description: >
  %TypedArray%.prototype.toSorted throws if the receiver is not a valid TypedArray
info: |
  %TypedArray%.prototype.toSorted ( comparefn )

  2. Let O be the this value.
  3. Perform ? ValidateTypedArray(O).
  ...
includes: [detachArrayBuffer.js, testTypedArray.js]
features: [TypedArray, change-array-by-copy]
---*/

var invalidValues = {
  'null': null,
  'undefined': undefined,
  'true': true,
  '"abc"': "abc",
  '12': 12,
  'Symbol()': Symbol(),
  '[1, 2, 3]': [1, 2, 3],
  '{ 0: 1, 1: 2, 2: 3, length: 3 }': { 0: 1, 1: 2, 2: 3, length: 3 },
  'Uint8Array.prototype': Uint8Array.prototype,
  '1n': 1n,
};

Object.entries(invalidValues).forEach(value => {
  assert.throws(TypeError, () => {
    TypedArray.prototype.toSorted.call(value[1]);
  }, `${value[0]} is not a valid TypedArray`);
});

testWithTypedArrayConstructors(function(TA) {
  let buffer = new ArrayBuffer(8);
  let sample = new TA(buffer, 0, 1);
  $DETACHBUFFER(sample.buffer);
  assert.throws(TypeError, () => {
    sample.toSorted();
  }, `array has a detached buffer`);
});
