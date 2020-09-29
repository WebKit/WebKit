// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype.item
description: >
  Array.prototype.item.name value and descriptor.
info: |
  Array.prototype.item( index )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
features: [Array.prototype.item]
---*/
assert.sameValue(typeof Array.prototype.item, 'function');

assert.sameValue(
  Array.prototype.item.name, 'item',
  'The value of Array.prototype.item.name is "item"'
);

verifyNotEnumerable(Array.prototype.item, 'name');
verifyNotWritable(Array.prototype.item, 'name');
verifyConfigurable(Array.prototype.item, 'name');
