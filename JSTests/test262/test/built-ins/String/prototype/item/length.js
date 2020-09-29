// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-string.prototype.item
description: >
  String.prototype.item.length value and descriptor.
info: |
  String.prototype.item( index )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
features: [String.prototype.item]
---*/
assert.sameValue(typeof String.prototype.item, 'function');

assert.sameValue(
  String.prototype.item.length, 1,
  'The value of String.prototype.item.length is 1'
);

verifyNotEnumerable(String.prototype.item, 'length');
verifyNotWritable(String.prototype.item, 'length');
verifyConfigurable(String.prototype.item, 'length');
