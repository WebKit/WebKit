// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.indexof
description: returns -1 if the element if not found
info: |
  22.2.3.13 %TypedArray%.prototype.indexOf (searchElement [ , fromIndex ] )

  %TypedArray%.prototype.indexOf is a distinct function that implements the same
  algorithm as Array.prototype.indexOf as defined in 22.1.3.12 except that the
  this object's [[ArrayLength]] internal slot is accessed in place of performing
  a [[Get]] of "length".

  22.1.3.12 Array.prototype.indexOf ( searchElement [ , fromIndex ] )

  ...
  6. If n ≥ 0, then
    a. If n is -0, let k be +0; else let k be n.
  7. Else n < 0,
    a. Let k be len + n.
    b. If k < 0, let k be 0.
  ...
  9. Return -1.
includes: [testTypedArray.js]
features: [TypedArray]
---*/

testWithTypedArrayConstructors(function(TA) {
  var sample;

  sample = new TA([42, 43, 42, 41]);
  assert.sameValue(sample.indexOf(44), -1, "indexOf(44)");
  assert.sameValue(sample.indexOf(43, 2), -1, "indexOf(43, 2)");
  assert.sameValue(sample.indexOf(42, 3), -1, "indexOf(42, 3)");
  assert.sameValue(sample.indexOf(44, -4), -1, "indexOf(44, -4)");
  assert.sameValue(sample.indexOf(44, -5), -1, "indexOf(44, -5)");
  assert.sameValue(sample.indexOf(42, -1), -1, "indexOf(42, -1)");
});
