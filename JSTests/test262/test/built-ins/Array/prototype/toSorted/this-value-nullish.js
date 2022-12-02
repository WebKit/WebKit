// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.toSorted
description: >
  Array.prototype.toSorted throws if the receiver is null or undefined
info: |
  Array.prototype.toSorted ( compareFn )

  1. Let O be ? ToObject(this value).
  ...
features: [change-array-by-copy]
---*/

assert.throws(TypeError, () => {
  Array.prototype.toSorted.call(null);
}, '`Array.prototype.toSorted.call(null)` throws TypeError');

assert.throws(TypeError, () => {
  Array.prototype.toSorted.call(undefined);
}, '`Array.prototype.toSorted.call(undefined)` throws TypeError');
