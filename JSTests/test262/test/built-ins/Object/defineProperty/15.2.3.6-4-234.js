// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-234
description: >
    Object.defineProperty - 'O' is an Array, 'name' is an array index
    property, the [[Enumerable]] field of 'desc' and the
    [[Enumerable]] attribute value of 'name' are two booleans with
    same value (15.4.5.1 step 4.c)
includes: [propertyHelper.js]
---*/

var arrObj = [];

Object.defineProperty(arrObj, "0", {
  enumerable: false
});

Object.defineProperty(arrObj, "0", {
  enumerable: false
});

verifyProperty(arrObj, "0", {
  value: undefined,
  writable: false,
  enumerable: false,
  configurable: false,
});
