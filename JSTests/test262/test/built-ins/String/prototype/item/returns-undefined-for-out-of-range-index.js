// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-string.prototype.item
description: >
  Creates an iterator from a custom object.
info: |
  String.prototype.item( index )

  If k < 0 or k ≥ len, then return undefined.
features: [String.prototype.item]
---*/
assert.sameValue(typeof String.prototype.item, 'function');

let s = "";

assert.sameValue(s.item(-2), undefined, 's.item(-2) must return undefined'); // wrap around the end
assert.sameValue(s.item(0), undefined, 's.item(0) must return undefined');
assert.sameValue(s.item(1), undefined, 's.item(1) must return undefined');

