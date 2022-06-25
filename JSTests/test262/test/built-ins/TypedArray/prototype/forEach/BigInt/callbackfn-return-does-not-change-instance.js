// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.foreach
description: >
  The callbackfn return does not change the instance
info: |
  22.2.3.12 %TypedArray%.prototype.forEach ( callbackfn [ , thisArg ] )

  %TypedArray%.prototype.forEach is a distinct function that implements the same
  algorithm as Array.prototype.forEach as defined in 22.1.3.10 except that the
  this object's [[ArrayLength]] internal slot is accessed in place of performing
  a [[Get]] of "length"
includes: [testBigIntTypedArray.js]
features: [BigInt, TypedArray]
---*/

testWithBigIntTypedArrayConstructors(function(TA) {
  var sample1 = new TA(3);

  sample1[1] = 1n;

  sample1.forEach(function() {
    return 42;
  });

  assert.sameValue(sample1[0], 0n, "[0] == 0");
  assert.sameValue(sample1[1], 1n, "[1] == 1");
  assert.sameValue(sample1[2], 0n, "[2] == 0");
});
