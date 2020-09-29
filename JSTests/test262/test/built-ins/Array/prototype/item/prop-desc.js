// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype.item
description: >
  Property type and descriptor.
info: |
  Array.prototype.item( index )

  17 ECMAScript Standard Built-in Objects
includes: [propertyHelper.js]
features: [Array.prototype.item]
---*/
assert.sameValue(typeof Array.prototype.item, 'function');

assert.sameValue(
  typeof Array.prototype.item,
  'function',
  'The value of `typeof Array.prototype.item` is "function"'
);

verifyNotEnumerable(Array.prototype, 'item');
verifyWritable(Array.prototype, 'item');
verifyConfigurable(Array.prototype, 'item');
