// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-string.prototype.item
description: >
  Property type and descriptor.
info: |
  String.prototype.item( index )

  Let relativeIndex be ? ToInteger(index).

features: [String.prototype.item]
---*/
assert.sameValue(typeof String.prototype.item, 'function');

let s = "01";

assert.throws(TypeError, () => {
  s.item(Symbol());
}, '`s.item(Symbol())` throws TypeError');
