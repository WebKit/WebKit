// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-array.of
description: >
  Array.of.length value and property descriptor
info: |
  Array.of ( ...items )

  The length property of the of function is 0.
includes: [propertyHelper.js]
---*/

assert.sameValue(
  Array.of.length, 0,
  'The value of Array.of.length is expected to be 0'
);

verifyNotEnumerable(Array.of, 'length');
verifyNotWritable(Array.of, 'length');
verifyConfigurable(Array.of, 'length');
