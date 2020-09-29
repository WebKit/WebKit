// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype.item
description: >
  Return abrupt from ToObject(this value).
info: |
  Array.prototype.item( index )

  Let O be ? ToObject(this value).

features: [Array.prototype.item]
---*/
assert.sameValue(typeof Array.prototype.item, 'function');

assert.throws(TypeError, () => {
  Array.prototype.item.call(undefined);
}, '`Array.prototype.item.call(undefined)` throws TypeError');

assert.throws(TypeError, () => {
  Array.prototype.item.call(null);
}, '`Array.prototype.item.call(null)` throws TypeError');
