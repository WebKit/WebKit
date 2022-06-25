// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.prototype.fill
description: >
  Array.prototype.fill.name value and descriptor.
info: |
  22.1.3.6 Array.prototype.fill (value [ , start [ , end ] ] )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  Array.prototype.fill.name, 'fill',
  'The value of `Array.prototype.fill.name` is `"fill"`'
);

verifyNotEnumerable(Array.prototype.fill, 'name');
verifyNotWritable(Array.prototype.fill, 'name');
verifyConfigurable(Array.prototype.fill, 'name');
