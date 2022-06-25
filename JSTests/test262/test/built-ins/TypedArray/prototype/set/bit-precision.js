// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.set.2
description: Preservation of bit-level encoding
info: |
  [...]
  28. Else,
      a. NOTE: If srcType and targetType are the same, the transfer must be
         performed in a manner that preserves the bit-level encoding of the
         source data.
      b. Repeat, while targetByteIndex < limit
         i. Let value be GetValueFromBuffer(srcBuffer, srcByteIndex, "Uint8").
         ii. Perform SetValueInBuffer(targetBuffer, targetByteIndex, "Uint8",
             value).
         iii. Set srcByteIndex to srcByteIndex + 1.
         iv. Set targetByteIndex to targetByteIndex + 1.
includes: [nans.js, compareArray.js, testTypedArray.js]
features: [TypedArray]
---*/

function body(FA) {
  var source = new FA(NaNs);
  var target = new FA(NaNs.length);
  var sourceBytes, targetBytes;

  target.set(source);

  sourceBytes = new Uint8Array(source.buffer);
  targetBytes = new Uint8Array(target.buffer);

  assert(compareArray(sourceBytes, targetBytes))
}

testWithTypedArrayConstructors(body, [Float32Array, Float64Array]);
