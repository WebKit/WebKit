// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.from
description: >
  Return a new TypedArray using a custom Constructor
includes: [testBigIntTypedArray.js]
features: [BigInt, TypedArray]
---*/

testWithBigIntTypedArrayConstructors(function(TA) {
  var called = 0;

  var ctor = function(len) {
    assert.sameValue(arguments.length, 1);
    called++;
    return new TA(len);
  };

  var result = TA.from.call(ctor, [42n, 43n, 42n]);
  assert.sameValue(result.length, 3);
  assert.sameValue(result[0], 42n);
  assert.sameValue(result[1], 43n);
  assert.sameValue(result[2], 42n);
  assert.sameValue(result.constructor, TA);
  assert.sameValue(Object.getPrototypeOf(result), TA.prototype);
  assert.sameValue(called, 1);
});
