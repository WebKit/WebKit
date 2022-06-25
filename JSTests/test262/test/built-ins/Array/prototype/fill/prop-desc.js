// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype.fill
description: Property type and descriptor.
info: |
  17 ECMAScript Standard Built-in Objects
includes: [propertyHelper.js]
---*/

assert.sameValue(
  typeof Array.prototype.fill,
  'function',
  '`typeof Array.prototype.fill` is `function`'
);

verifyNotEnumerable(Array.prototype, 'fill');
verifyWritable(Array.prototype, 'fill');
verifyConfigurable(Array.prototype, 'fill');
