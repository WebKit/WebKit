// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype.fill
description: Array.prototype.fill.length value and descriptor.
info: |
  17 ECMAScript Standard Built-in Objects
includes: [propertyHelper.js]
---*/

assert.sameValue(
  Array.prototype.fill.length, 1,
  'The value of `Array.prototype.fill.length` is `1`'
);

verifyNotEnumerable(Array.prototype.fill, 'length');
verifyNotWritable(Array.prototype.fill, 'length');
verifyConfigurable(Array.prototype.fill, 'length');
