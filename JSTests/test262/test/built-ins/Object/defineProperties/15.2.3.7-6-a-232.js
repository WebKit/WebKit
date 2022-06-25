// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.7-6-a-232
description: >
    Object.defineProperties - 'O' is an Array, 'P' is an array index
    property,  'P' is accessor property and 'desc' is data descriptor,
    and the [[Configurable]] attribute value of 'P' is true, test 'P'
    is converted from accessor property to data property  (15.4.5.1
    step 4.c)
includes: [propertyHelper.js]
---*/

var arr = [];

Object.defineProperty(arr, "1", {
  get: function() {
    return 3;
  },
  configurable: true

});

Object.defineProperties(arr, {
  "1": {
    value: 12
  }
});

verifyEqualTo(arr, "1", 12);

verifyNotWritable(arr, "1");

verifyNotEnumerable(arr, "1");

verifyConfigurable(arr, "1");
