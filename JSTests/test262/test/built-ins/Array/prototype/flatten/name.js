// Copyright (C) 2018 Shilpi Jain and Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype.flatten
description: >
  Array.prototype.flatten.name value and descriptor.
info: >
  17 ECMAScript Standard Built-in Objects
includes: [propertyHelper.js]
features: [Array.prototype.flatten]
---*/

assert.sameValue(
  Array.prototype.flatten.name, 'flatten',
  'The value of `Array.prototype.flatten.name` is `"flatten"`'
);

verifyNotEnumerable(Array.prototype.flatten, 'name');
verifyNotWritable(Array.prototype.flatten, 'name');
verifyConfigurable(Array.prototype.flatten, 'name');
