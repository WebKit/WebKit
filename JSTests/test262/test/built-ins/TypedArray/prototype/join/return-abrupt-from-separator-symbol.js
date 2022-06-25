// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.join
description: Return abrupt from ToString(Symbol separator)
info: |
  22.2.3.15 %TypedArray%.prototype.join ( separator )

  %TypedArray%.prototype.join is a distinct function that implements the same
  algorithm as Array.prototype.join as defined in 22.1.3.13 except that the this
  object's [[ArrayLength]] internal slot is accessed in place of performing a
  [[Get]] of "length".

  22.1.3.13 Array.prototype.join (separator)

  ...
  4. Let sep be ? ToString(separator).
  5. If len is zero, return the empty String.
  ...
includes: [testTypedArray.js]
features: [Symbol, TypedArray]
---*/

var s = Symbol("");

testWithTypedArrayConstructors(function(TA) {
  var sample = new TA();

  assert.throws(TypeError, function() {
    sample.join(s);
  });
});
