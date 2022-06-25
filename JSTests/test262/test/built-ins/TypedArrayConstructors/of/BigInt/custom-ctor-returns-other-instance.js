// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-%typedarray%.of
description: >
  Custom constructor can return any TypedArray instance with higher or same
  length
info: |
  %TypedArray%.of ( ...items )

  1. Let len be the actual number of arguments passed to this function.
  ...
  5. Let newObj be ? TypedArrayCreate(C, « len »).
  ...
includes: [testBigIntTypedArray.js]
features: [BigInt, TypedArray]
---*/

testWithBigIntTypedArrayConstructors(function(TA) {
  var result;
  var custom = new TA(3);
  var ctor = function() {
    return custom;
  };

  result = TypedArray.of.call(ctor, 1n, 2n, 3n);
  assert.sameValue(result, custom, "using iterator, same length");

  result = TypedArray.of.call(ctor, 1n, 2n);
  assert.sameValue(result, custom, "using iterator, higher length");
});
