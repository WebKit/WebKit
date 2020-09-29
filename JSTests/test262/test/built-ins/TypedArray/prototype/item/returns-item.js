// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.item
description: >
  Returns the item value at the specified index
info: |
  %TypedArray%.prototype.item( index )

  Let O be the this value.
  Perform ? ValidateTypedArray(O).
  Let len be O.[[ArrayLength]].
  Let relativeIndex be ? ToInteger(index).
  If relativeIndex ≥ 0, then
    Let k be relativeIndex.
  Else,
    Let k be len + relativeIndex.
  If k < 0 or k ≥ len, then return undefined.
  Return ? Get(O, ! ToString(k)).

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
  let a = new TA([1, 2, 3, 4]);

  assert.sameValue(a.item(0), 1, 'a.item(0) must return 1');
  assert.sameValue(a.item(1), 2, 'a.item(1) must return 2');
  assert.sameValue(a.item(2), 3, 'a.item(2) must return 3');
  assert.sameValue(a.item(3), 4, 'a.item(3) must return 4');
});
