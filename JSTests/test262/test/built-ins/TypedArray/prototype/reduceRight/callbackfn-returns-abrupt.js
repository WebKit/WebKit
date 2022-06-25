// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.reduceright
description: >
  Returns abrupt from callbackfn
info: |
  22.2.3.21 %TypedArray%.prototype.reduceRight ( callbackfn [ , initialValue ] )

  %TypedArray%.prototype.reduceRight is a distinct function that implements the
  same algorithm as Array.prototype.reduceRight as defined in 22.1.3.20 except
  that the this object's [[ArrayLength]] internal slot is accessed in place of
  performing a [[Get]] of "length".

  22.1.3.20 Array.prototype.reduceRight ( callbackfn [ , initialValue ] )

  ...
  8. Repeat, while k < len
    ...
    c. If kPresent is true, then
      ...
      i. Let accumulator be ? Call(callbackfn, undefined, « accumulator, kValue,
      k, O »).
  ...
includes: [testTypedArray.js]
features: [TypedArray]
---*/

testWithTypedArrayConstructors(function(TA) {
  var sample = new TA(2);

  assert.throws(Test262Error, function() {
    sample.reduceRight(function() {
      throw new Test262Error();
    });
  });

  assert.throws(Test262Error, function() {
    sample.reduceRight(function() {
      throw new Test262Error();
    }, 0);
  });
});
