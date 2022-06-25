// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.reduceright
description: >
  callbackfn arguments using custom accumulator
info: |
  22.2.3.21 %TypedArray%.prototype.reduceRight ( callbackfn [ , initialValue ] )

  %TypedArray%.prototype.reduceRight is a distinct function that implements the
  same algorithm as Array.prototype.reduceRight as defined in 22.1.3.20 except
  that the this object's [[ArrayLength]] internal slot is accessed in place of
  performing a [[Get]] of "length".

  22.1.3.20 Array.prototype.reduceRight ( callbackfn [ , initialValue ] )

  ...
  8. Repeat, while k ≥ 0
    ...
    c. If kPresent is true, then
      i. Let kValue be ? Get(O, Pk).
      ii. Let accumulator be ? Call(callbackfn, undefined, « accumulator,
      kValue, k, O »).
    d. Decrease k by 1.
  ...
includes: [testBigIntTypedArray.js]
features: [BigInt, TypedArray]
---*/

testWithBigIntTypedArrayConstructors(function(TA) {
  var sample = new TA([42n, 43n, 44n]);

  var results = [];

  sample.reduceRight(function(accumulator) {
    results.push(arguments);
    return accumulator + 1;
  }, 7);

  assert.sameValue(results.length, 3, "results.length");

  assert.sameValue(results[0].length, 4, "results[0].length");
  assert.sameValue(results[0][0], 7, "results[0][0] - accumulator");
  assert.sameValue(results[0][1], 44n, "results[0][1] - kValue");
  assert.sameValue(results[0][2], 2, "results[0][2] - k");
  assert.sameValue(results[0][3], sample, "results[0][3] - this");

  assert.sameValue(results[1].length, 4, "results[1].length");
  assert.sameValue(results[1][0], 8, "results[1][0] - accumulator");
  assert.sameValue(results[1][1], 43n, "results[1][1] - kValue");
  assert.sameValue(results[1][2], 1, "results[1][2] - k");
  assert.sameValue(results[1][3], sample, "results[1][3] - this");

  assert.sameValue(results[2].length, 4, "results[2].length");
  assert.sameValue(results[2][0], 9, "results[2][0] - accumulator");
  assert.sameValue(results[2][1], 42n, "results[2][1] - kValue");
  assert.sameValue(results[2][2], 0, "results[2][2] - k");
  assert.sameValue(results[2][3], sample, "results[2][3] - this");
});
