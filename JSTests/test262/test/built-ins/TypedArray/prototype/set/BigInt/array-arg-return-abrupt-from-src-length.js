// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.set-array-offset
description: >
  Return abrupt from ToLength(src "length")
info: |
  22.2.3.23.1 %TypedArray%.prototype.set (array [ , offset ] )

  1. Assert: array is any ECMAScript language value other than an Object with a
  [[TypedArrayName]] internal slot. If it is such an Object, the definition in
  22.2.3.23.2 applies.
  ...
  16. Let srcLength be ? ToLength(? Get(src, "length")).
  ...
includes: [testBigIntTypedArray.js]
features: [BigInt, TypedArray]
---*/

var obj1 = {
  length: {
    valueOf: function() {
      throw new Test262Error();
    }
  }
};

var obj2 = {
  length: {
    toString: function() {
      throw new Test262Error();
    }
  }
};

testWithBigIntTypedArrayConstructors(function(TA) {
  var sample = new TA([1n, 2n, 3n]);

  assert.throws(Test262Error, function() {
    sample.set(obj1);
  }, "valueOf");

  assert.throws(Test262Error, function() {
    sample.set(obj2);
  }, "toString");
});
