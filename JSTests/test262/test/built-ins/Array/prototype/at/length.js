// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype.at
description: >
  Array.prototype.at.length value and descriptor.
info: |
  Array.prototype.at( index )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
features: [Array.prototype.at]
---*/
assert.sameValue(
  typeof Array.prototype.at,
  'function',
  'The value of `typeof Array.prototype.at` is expected to be "function"'
);

assert.sameValue(
  Array.prototype.at.length, 1,
  'The value of Array.prototype.at.length is expected to be 1'
);

verifyNotEnumerable(Array.prototype.at, 'length');
verifyNotWritable(Array.prototype.at, 'length');
verifyConfigurable(Array.prototype.at, 'length');
