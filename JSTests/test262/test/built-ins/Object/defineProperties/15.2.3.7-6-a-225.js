// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.7-6-a-225
description: >
    Object.defineProperties - 'O' is an Array, 'P' is an array index
    property that already exists on 'O' with [[Configurable]] true,
    the [[Configurable]] field of 'desc' is true (15.4.5.1 step 4.c)
includes: [propertyHelper.js]
---*/

var arr = [];

Object.defineProperty(arr, "0", {
  configurable: true
});

Object.defineProperties(arr, {
  "0": {
    configurable: true
  }
});
verifyEqualTo(arr, "0", undefined);

verifyNotWritable(arr, "0");

verifyNotEnumerable(arr, "0");

verifyConfigurable(arr, "0");
