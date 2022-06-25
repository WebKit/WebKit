// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.lastindexof
description: returns -1 if the element if not found
info: |
  22.2.3.17 %TypedArray%.prototype.lastIndexOf ( searchElement [ , fromIndex ] )

  %TypedArray%.prototype.lastIndexOf is a distinct function that implements the
  same algorithm as Array.prototype.lastIndexOf as defined in 22.1.3.15 except
  that the this object's [[ArrayLength]] internal slot is accessed in place of
  performing a [[Get]] of "length".

  22.1.3.15 Array.prototype.lastIndexOf ( searchElement [ , fromIndex ] )

  ...
  5. If n ≥ 0, then
    a. If n is -0, let k be +0; else let k be min(n, len - 1).
  6. Else n < 0,
    a. Let k be len + n.
  7. Repeat, while k ≥ 0
    ...
  8. Return -1.
includes: [testTypedArray.js]
features: [TypedArray]
---*/

testWithTypedArrayConstructors(function(TA) {
  var sample;

  sample = new TA([42, 43, 42, 41]);
  assert.sameValue(sample.lastIndexOf(44), -1, "lastIndexOf(44)");
  assert.sameValue(sample.lastIndexOf(44, -4), -1, "lastIndexOf(44, -4)");
  assert.sameValue(sample.lastIndexOf(44, -5), -1, "lastIndexOf(44, -5)");
  assert.sameValue(sample.lastIndexOf(42, -5), -1, "lastIndexOf(42, -5)");
  assert.sameValue(sample.lastIndexOf(43, -4), -1, "lastIndexOf(43, -4)");
  assert.sameValue(sample.lastIndexOf(43, -5), -1, "lastIndexOf(43, -5)");
  assert.sameValue(sample.lastIndexOf(41, 0), -1, "lastIndexOf(41, 0)");
  assert.sameValue(sample.lastIndexOf(41, 1), -1, "lastIndexOf(41, 1)");
  assert.sameValue(sample.lastIndexOf(41, 2), -1, "lastIndexOf(41, 2)");
  assert.sameValue(sample.lastIndexOf(43, 0), -1, "lastIndexOf(43, 0)");
});
