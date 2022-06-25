// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-222
description: >
    Object.defineProperty - 'O' is an Array, 'name' is an array index
    property, the [[Value]] field of 'desc' and the [[Value]]
    attribute value of 'name' are two strings which have same length
    and same characters in corresponding positions (15.4.5.1 step 4.c)
includes: [propertyHelper.js]
---*/

var arrObj = [];

Object.defineProperty(arrObj, "0", {
  value: "abcd"
});

Object.defineProperty(arrObj, "0", {
  value: "abcd"
});
verifyEqualTo(arrObj, "0", "abcd");

verifyNotWritable(arrObj, "0");

verifyNotEnumerable(arrObj, "0");

verifyNotConfigurable(arrObj, "0");
