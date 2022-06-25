// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-216
description: >
    Object.defineProperty - 'O' is an Array, 'name' is an array index
    property, both the [[Value]] field of 'desc' and the [[Value]]
    attribute value of 'name' are null  (15.4.5.1 step 4.c)
includes: [propertyHelper.js]
---*/

var arrObj = [];

Object.defineProperty(arrObj, "0", {
  value: null
});

Object.defineProperty(arrObj, "0", {
  value: null
});
verifyEqualTo(arrObj, "0", null);

verifyNotWritable(arrObj, "0");

verifyNotEnumerable(arrObj, "0");

verifyNotConfigurable(arrObj, "0");
