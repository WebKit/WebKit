// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype.item
description: >
  Property type and descriptor.
info: |
  Array.prototype.item( index )

  Let relativeIndex be ? ToInteger(index).

features: [Array.prototype.item]
---*/
assert.sameValue(typeof Array.prototype.item, 'function');

let a = [0,1,2,3];

assert.throws(TypeError, () => {
  a.item(Symbol());
}, '`a.item(Symbol())` throws TypeError');
