// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.copywithin
description: >
  Return abrupt from ToInteger(target).
info: |
  22.2.3.5 %TypedArray%.prototype.copyWithin (target, start [ , end ] )

  %TypedArray%.prototype.copyWithin is a distinct function that implements the
  same algorithm as Array.prototype.copyWithin as defined in 22.1.3.3 except
  that the this object's [[ArrayLength]] internal slot is accessed in place of
  performing a [[Get]] of "length" and the actual copying of values in step 12
  must be performed in a manner that preserves the bit-level encoding of the
  source data.

  ...

  22.1.3.3 Array.prototype.copyWithin (target, start [ , end ] )

  ...
  3. Let relativeTarget be ? ToInteger(target).
  ...
includes: [testBigIntTypedArray.js]
features: [BigInt, TypedArray]
---*/

var o = {
  valueOf: function() {
    throw new Test262Error();
  }
};

testWithBigIntTypedArrayConstructors(function(TA) {
  var sample = new TA();
  assert.throws(Test262Error, function() {
    sample.copyWithin(o);
  });
});
