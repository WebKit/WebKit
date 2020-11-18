// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-string.prototype.at
description: >
  Return abrupt from RequireObjectCoercible(this value).
info: |
  String.prototype.at( index )

  Let O be ? RequireObjectCoercible(this value).

features: [String.prototype.at]
---*/
assert.sameValue(typeof String.prototype.at, 'function');

assert.throws(TypeError, () => {
  String.prototype.at.call(undefined);
}, '`String.prototype.at.call(undefined)` throws TypeError');

assert.throws(TypeError, () => {
  String.prototype.at.call(null);
}, '`String.prototype.at.call(null)` throws TypeError');
