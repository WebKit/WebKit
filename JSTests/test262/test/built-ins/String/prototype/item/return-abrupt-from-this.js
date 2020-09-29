// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-string.prototype.item
description: >
  Return abrupt from RequireObjectCoercible(this value).
info: |
  String.prototype.item( index )

  Let O be ? RequireObjectCoercible(this value).

features: [String.prototype.item]
---*/
assert.sameValue(typeof String.prototype.item, 'function');

assert.throws(TypeError, () => {
  String.prototype.item.call(undefined);
}, '`String.prototype.item.call(undefined)` throws TypeError');

assert.throws(TypeError, () => {
  String.prototype.item.call(null);
}, '`String.prototype.item.call(null)` throws TypeError');
