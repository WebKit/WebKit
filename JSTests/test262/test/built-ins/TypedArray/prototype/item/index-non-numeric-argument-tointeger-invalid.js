// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.item
description: >
  Property type and descriptor.
info: |
  %TypedArray%.prototype.item( index )

  Let relativeIndex be ? ToInteger(index).

includes: [testTypedArray.js]
features: [TypedArray, TypedArray.prototype.item]
---*/
assert.sameValue(
  typeof TypedArray.prototype.item,
  'function',
  'The value of `typeof TypedArray.prototype.item` is "function"'
);

testWithTypedArrayConstructors(TA => {
  assert.sameValue(typeof TA.prototype.item, 'function', 'The value of `typeof TA.prototype.item` is "function"');
  let a = new TA([0,1,2,3]);

  assert.throws(TypeError, () => {
    a.item(Symbol());
  }, '`a.item(Symbol())` throws TypeError');
});
