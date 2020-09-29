// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-string.prototype.item
description: >
  Property type and descriptor.
info: |
  String.prototype.item( index )

  17 ECMAScript Standard Built-in Objects
includes: [propertyHelper.js]
features: [String.prototype.item]
---*/
assert.sameValue(typeof String.prototype.item, 'function');

assert.sameValue(
  typeof String.prototype.item,
  'function',
  'The value of `typeof String.prototype.item` is "function"'
);

verifyNotEnumerable(String.prototype, 'item');
verifyWritable(String.prototype, 'item');
verifyConfigurable(String.prototype, 'item');
