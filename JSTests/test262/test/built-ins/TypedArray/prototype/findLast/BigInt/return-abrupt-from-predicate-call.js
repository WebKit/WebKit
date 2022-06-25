// Copyright (C) 2021 Microsoft. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.findlast
description: >
  Return abrupt from predicate call.
info: |
  %TypedArray%.prototype.findLast (predicate [ , thisArg ] )

  6. Repeat, while k ≥ 0,
  ...
    c. Let testResult be ! ToBoolean(? Call(predicate, thisArg, « kValue, 𝔽(k), O »)).
  ...
includes: [testBigIntTypedArray.js]
features: [BigInt, TypedArray, array-find-from-last]
---*/

testWithBigIntTypedArrayConstructors(function(TA) {
  var sample = new TA(1);

  var predicate = function() {
    throw new Test262Error();
  };

  assert.throws(Test262Error, function() {
    sample.findLast(predicate);
  });
});
