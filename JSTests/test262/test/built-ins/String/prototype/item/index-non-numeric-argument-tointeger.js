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

assert.sameValue(s.item(false), '0', 's.item(false) must return 0');
assert.sameValue(s.item(null), '0', 's.item(null) must return 0');
assert.sameValue(s.item(undefined), '0', 's.item(undefined) must return 0');
assert.sameValue(s.item(""), '0', 's.item("") must return 0');
assert.sameValue(s.item(function() {}), '0', 's.item(function() {}) must return 0');
assert.sameValue(s.item([]), '0', 's.item([]) must return 0');

assert.sameValue(s.item(true), '1', 's.item(true) must return 1');
assert.sameValue(s.item("1"), '1', 's.item("1") must return 1');
