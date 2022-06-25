// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-49
description: >
    Object.defineProperty - 'name' property doesn't exist in 'O', test
    [[Enumerable]] of 'name' property of 'Attributes' is set as false
    value if absent in data descriptor 'desc' (8.12.9 step 4.a.i)
includes: [propertyHelper.js]
---*/

var obj = {};

Object.defineProperty(obj, "property", {
  value: 1001,
  writable: true,
  configurable: true
});
verifyEqualTo(obj, "property", 1001);

verifyWritable(obj, "property");

verifyNotEnumerable(obj, "property");

verifyConfigurable(obj, "property");
