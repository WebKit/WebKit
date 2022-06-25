// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.7-6-a-258
description: >
    Object.defineProperties - 'O' is an Array, 'P' is an array index
    named property that already exists on 'O' is accessor property and
    'desc' is accessor descriptor, test setting the [[Set]] attribute
    value of 'P' as undefined  (15.4.5.1 step 4.c)
includes: [propertyHelper.js]
---*/


var arr = [];

Object.defineProperty(arr, "0", {
  set: function() {},
  configurable: true
});

Object.defineProperties(arr, {
  "0": {
    set: undefined
  }
});
verifyNotEnumerable(arr, "0");

verifyConfigurable(arr, "0");
