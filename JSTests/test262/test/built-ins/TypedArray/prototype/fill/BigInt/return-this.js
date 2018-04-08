// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.fill
description: >
  Returns `this`.
includes: [testBigIntTypedArray.js]
features: [BigInt, TypedArray]
---*/

testWithBigIntTypedArrayConstructors(function(TA) {
  var sample1 = new TA();
  var result1 = sample1.fill(1n);

  assert.sameValue(result1, sample1);

  var sample2 = new TA(42);
  var result2 = sample2.fill(7n);
  assert.sameValue(result2, sample2);
});
