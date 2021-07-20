// Copyright (C) 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.foreach
description: Return abrupt when "this" value fails buffer boundary checks
includes: [testTypedArray.js]
features: [TypedArray, resizable-arraybuffer]
---*/

assert.sameValue(
  typeof TypedArray.prototype.forEach,
  'function',
  'implements TypedArray.prototype.forEach'
);

assert.sameValue(
  typeof ArrayBuffer.prototype.resize,
  'function',
  'implements ArrayBuffer.prototype.resize'
);

testWithTypedArrayConstructors(TA => {
  var BPE = TA.BYTES_PER_ELEMENT;
  var ab = new ArrayBuffer(BPE * 4, {maxByteLength: BPE * 5});
  var array = new TA(ab, BPE, 2);

  try {
    ab.resize(BPE * 5);
  } catch (_) {}

  // no error following grow:
  array.forEach(() => {});

  try {
    ab.resize(BPE * 3);
  } catch (_) {}

  // no error following shrink (within bounds):
  array.forEach(() => {});

  var expectedError;
  try {
    ab.resize(BPE * 3);
    expectedError = TypeError;
  } catch (_) {
    // The host is permitted to fail any "resize" operation at its own
    // discretion. If that occurs, the forEach operation should complete
    // successfully.
    expectedError = Test262Error;
  }

  assert.throws(expectedError, () => {
    array.forEach(() => {});
    throw new Test262Error('forEach completed successfully');
  });
});
