// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-199
description: >
    Object.defineProperty - 'O' is an Array, 'name' is an array index
    named property, 'name' property doesn't exist in 'O', test 'name'
    is defined as data property when 'desc' is generic descriptor
    (15.4.5.1 step 4.c)
includes: [propertyHelper.js]
---*/

var arrObj = [];

Object.defineProperty(arrObj, "0", {
  enumerable: true
});

verifyEqualTo(arrObj, "0", undefined);

verifyNotWritable(arrObj, "0");

verifyEnumerable(arrObj, "0");

verifyNotConfigurable(arrObj, "0");
