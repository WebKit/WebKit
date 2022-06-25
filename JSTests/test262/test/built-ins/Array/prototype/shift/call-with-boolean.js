// Copyright (c) 2020 Rick Waldron.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.shift
description: Array.prototype.shift applied to boolean primitive
---*/

assert.sameValue(
  Array.prototype.shift.call(true),
  undefined,
  'Array.prototype.shift.call(true) must return undefined'
);
assert.sameValue(
  Array.prototype.shift.call(false),
  undefined,
  'Array.prototype.shift.call(false) must return undefined'
);
