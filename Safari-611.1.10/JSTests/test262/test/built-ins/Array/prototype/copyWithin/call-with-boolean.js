// Copyright (c) 2020 Rick Waldron.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.copyWithin
description: Array.prototype.copyWithin applied to boolean primitive
---*/

assert.sameValue(
  Array.prototype.copyWithin.call(true) instanceof Boolean,
  true,
  'The result of `(Array.prototype.copyWithin.call(true) instanceof Boolean)` is true'
);
assert.sameValue(
  Array.prototype.copyWithin.call(false) instanceof Boolean,
  true,
  'The result of `(Array.prototype.copyWithin.call(false) instanceof Boolean)` is true'
);
