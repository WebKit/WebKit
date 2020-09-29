// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.item
description: >
  Returns undefined if the specified index less than or greater than the available index range.
info: |
  %TypedArray%.prototype.item( index )

  If k < 0 or k ≥ len, then return undefined.

includes: [testTypedArray.js]
features: [TypedArray,TypedArray.prototype.item]
---*/
assert.sameValue(
  typeof TypedArray.prototype.item,
  'function',
  'The value of `typeof TypedArray.prototype.item` is "function"'
);

testWithTypedArrayConstructors(TA => {
  assert.sameValue(typeof TA.prototype.item, 'function', 'The value of `typeof TA.prototype.item` is "function"');
  let a = new TA([]);

  assert.sameValue(a.item(-2), undefined, 'a.item(-2) must return undefined'); // wrap around the end
  assert.sameValue(a.item(0), undefined, 'a.item(0) must return undefined');
  assert.sameValue(a.item(1), undefined, 'a.item(1) must return undefined');
});
