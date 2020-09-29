// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-string.prototype.item
description: >
  String.prototype.item.name value and descriptor.
info: |
  String.prototype.item( index )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
features: [String.prototype.item]
---*/
assert.sameValue(typeof String.prototype.item, 'function');

assert.sameValue(
  String.prototype.item.name, 'item',
  'The value of String.prototype.item.name is "item"'
);

verifyNotEnumerable(String.prototype.item, 'name');
verifyNotWritable(String.prototype.item, 'name');
verifyConfigurable(String.prototype.item, 'name');
