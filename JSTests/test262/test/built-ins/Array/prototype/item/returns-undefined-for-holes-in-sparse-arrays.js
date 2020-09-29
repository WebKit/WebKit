// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype.item
description: >
  Returns the item value at the specified index, respecting holes in sparse arrays.
info: |
  Array.prototypeitem ( )

  Let O be ? ToObject(this value).
  Let len be ? LengthOfArrayLike(O).
  Let relativeIndex be ? ToInteger(index).
  If relativeIndex ≥ 0, then
    Let k be relativeIndex.
  Else,
    Let k be len + relativeIndex.
  If k < 0 or k ≥ len, then return undefined.
  Return ? Get(O, ! ToString(k)).

features: [Array.prototype.item]
---*/
assert.sameValue(
  typeof Array.prototype.item,
  'function',
  'The value of `typeof Array.prototype.item` is "function"'
);

let a = [0, 1, , 3, 4, , 6];

assert.sameValue(a.item(0), 0, 'a.item(0) must return 0');
assert.sameValue(a.item(1), 1, 'a.item(1) must return 1');
assert.sameValue(a.item(2), undefined, 'a.item(2) must return undefined');
assert.sameValue(a.item(3), 3, 'a.item(3) must return 3');
assert.sameValue(a.item(4), 4, 'a.item(4) must return 4');
assert.sameValue(a.item(5), undefined, 'a.item(5) must return undefined');
assert.sameValue(a.item(6), 6, 'a.item(6) must return 6');
assert.sameValue(a.item(-0), 0, 'a.item(-0) must return 0');
assert.sameValue(a.item(-1), 6, 'a.item(-1) must return 6');
assert.sameValue(a.item(-2), undefined, 'a.item(-2) must return undefined');
assert.sameValue(a.item(-3), 4, 'a.item(-3) must return 4');
assert.sameValue(a.item(-4), 3, 'a.item(-4) must return 3');
assert.sameValue(a.item(-5), undefined, 'a.item(-5) must return undefined');
assert.sameValue(a.item(-6), 1, 'a.item(-6) must return 1');
