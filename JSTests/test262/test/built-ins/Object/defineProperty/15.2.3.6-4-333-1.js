// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-333-1
description: >
    Object.defineProperty will update [[Value]] attribute of named
    property 'P' successfully when [[Configurable]] attribute is
    false, [[Writable]] attribute is true and 'O' is an Object object
    (8.12.9 - step 10)
includes: [propertyHelper.js]
---*/


var obj = {};

Object.defineProperty(obj, "property", {
  value: 1001,
  writable: true,
  configurable: false
});

Object.defineProperty(obj, "property", {
  value: 1002
});

verifyEqualTo(obj, "property", 1002);

verifyWritable(obj, "property");

verifyNotEnumerable(obj, "property");

verifyNotConfigurable(obj, "property");
