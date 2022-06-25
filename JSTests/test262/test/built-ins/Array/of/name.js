// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.of
description: >
  Array.of.name value and property descriptor
info: |
  Array.of ( ...items )

  17 ECMAScript Standard Built-in Objects

includes: [propertyHelper.js]
---*/

assert.sameValue(
  Array.of.name, 'of',
  'The value of Array.of.name is expected to be "of"'
);

verifyNotEnumerable(Array.of, 'name');
verifyNotWritable(Array.of, 'name');
verifyConfigurable(Array.of, 'name');
