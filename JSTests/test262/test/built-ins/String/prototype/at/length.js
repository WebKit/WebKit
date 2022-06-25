// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-string.prototype.at
description: >
  String.prototype.at.length value and descriptor.
info: |
  String.prototype.at( index )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
features: [String.prototype.at]
---*/
assert.sameValue(typeof String.prototype.at, 'function');

assert.sameValue(
  String.prototype.at.length, 1,
  'The value of String.prototype.at.length is 1'
);

verifyNotEnumerable(String.prototype.at, 'length');
verifyNotWritable(String.prototype.at, 'length');
verifyConfigurable(String.prototype.at, 'length');
