// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype.item
description: >
  Returns undefined if the specified index less than or greater than the available index range.
info: |
  Array.prototype.item( index )

  If k < 0 or k ≥ len, then return undefined.
features: [Array.prototype.item]
---*/
assert.sameValue(typeof Array.prototype.item, 'function');

let a = [];

assert.sameValue(a.item(-2), undefined, 'a.item(-2) must return undefined'); // wrap around the end
assert.sameValue(a.item(0), undefined, 'a.item(0) must return undefined');
assert.sameValue(a.item(1), undefined, 'a.item(1) must return undefined');

