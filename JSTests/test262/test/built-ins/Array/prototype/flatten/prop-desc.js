// Copyright (C) 2018 Shilpi Jain and Michael Ficarra. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype.flatten
es6id: 22.1.3
description: Property type and descriptor.
info: >
  17 ECMAScript Standard Built-in Objects
includes: [propertyHelper.js]
features: [Array.prototype.flatten]
---*/

assert.sameValue(
  typeof Array.prototype.flatten,
  'function',
  '`typeof Array.prototype.flatten` is `function`'
);

verifyNotEnumerable(Array.prototype, 'flatten');
verifyWritable(Array.prototype, 'flatten');
verifyConfigurable(Array.prototype, 'flatten');
