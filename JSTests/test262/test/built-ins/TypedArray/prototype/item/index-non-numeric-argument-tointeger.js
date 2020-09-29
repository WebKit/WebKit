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

  assert.sameValue(a.item(false), 0, 'a.item(false) must return 0');
  assert.sameValue(a.item(null), 0, 'a.item(null) must return 0');
  assert.sameValue(a.item(undefined), 0, 'a.item(undefined) must return 0');
  assert.sameValue(a.item(""), 0, 'a.item("") must return 0');
  assert.sameValue(a.item(function() {}), 0, 'a.item(function() {}) must return 0');
  assert.sameValue(a.item([]), 0, 'a.item([]) must return 0');

  assert.sameValue(a.item(true), 1, 'a.item(true) must return 1');
  assert.sameValue(a.item("1"), 1, 'a.item("1") must return 1');
});
