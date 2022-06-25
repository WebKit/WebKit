// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-%typedarray%.prototype.includes
description: Return abrupt from ToInteger(fromIndex)
info: |
  22.2.3.13 %TypedArray%.prototype.includes ( searchElement [ , fromIndex ] )

  %TypedArray%.prototype.includes is a distinct function that implements the
  same algorithm as Array.prototype.includes as defined in 22.1.3.11 except that
  the this object's [[ArrayLength]] internal slot is accessed in place of
  performing a [[Get]] of "length".

  22.1.3.11 Array.prototype.includes ( searchElement [ , fromIndex ] )

  ...
  4. Let n be ? ToInteger(fromIndex). (If fromIndex is undefined, this step
  produces the value 0.)
  ...
includes: [testBigIntTypedArray.js]
features: [BigInt, TypedArray]
---*/

var fromIndex = {
  valueOf: function() {
    throw new Test262Error();
  }
};

testWithBigIntTypedArrayConstructors(function(TA) {
  var sample = new TA([7n]);

  assert.throws(Test262Error, function() {
    sample.includes(7n, fromIndex);
  });
});
