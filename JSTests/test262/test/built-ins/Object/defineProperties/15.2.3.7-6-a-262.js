// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.7-6-a-262
description: >
    Object.defineProperties - 'O' is an Array, 'P' is an array index
    named property that already exists on 'O' is accessor property and
    'desc' is accessor descriptor, test updating multiple attribute
    values of 'P'  (15.4.5.1 step 4.c)
includes: [propertyHelper.js]
---*/


var arr = [];

function get_fun() {
  return 36;
}

function set_fun(value) {
  arr.setVerifyHelpProp = value;
}
Object.defineProperty(arr, "0", {
  get: function() {
    return 12;
  },
  set: set_fun,
  enumerable: true,
  configurable: true
});

Object.defineProperties(arr, {
  "0": {
    get: get_fun,
    enumerable: false,
    configurable: false
  }
});
verifyEqualTo(arr, "0", get_fun());

verifyWritable(arr, "0", "setVerifyHelpProp");

verifyNotEnumerable(arr, "0");

verifyNotConfigurable(arr, "0");
