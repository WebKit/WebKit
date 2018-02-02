// Copyright (C) 2018 Shilpi Jain and Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype.flatten
description: Array.prototype.flatten.length value and descriptor.
info: >
  17 ECMAScript Standard Built-in Objects
includes: [propertyHelper.js]
features: [Array.prototype.flatten]
---*/

assert.sameValue(
  Array.prototype.flatten.length, 0,
  'The value of `Array.prototype.flatten.length` is `0`'
);

verifyNotEnumerable(Array.prototype.flatten, 'length');
verifyNotWritable(Array.prototype.flatten, 'length');
verifyConfigurable(Array.prototype.flatten, 'length');
