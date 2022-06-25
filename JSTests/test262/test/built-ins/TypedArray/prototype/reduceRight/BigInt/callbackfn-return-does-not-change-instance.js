// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.reduceright
description: >
  The callbackfn return does not change the `this` instance
includes: [testBigIntTypedArray.js]
features: [BigInt, TypedArray]
---*/

testWithBigIntTypedArrayConstructors(function(TA) {
  var sample = new TA([0n, 1n, 0n]);

  sample.reduceRight(function() {
    return 42;
  }, 7);

  assert.sameValue(sample[0], 0n, "[0] == 0");
  assert.sameValue(sample[1], 1n, "[1] == 1");
  assert.sameValue(sample[2], 0n, "[2] == 0");
});
