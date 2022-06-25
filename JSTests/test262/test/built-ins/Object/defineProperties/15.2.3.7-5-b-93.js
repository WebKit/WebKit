// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.7-5-b-93
description: >
    Object.defineProperties - value of 'configurable' property of
    'descObj' is positive number (8.10.5 step 4.b)
---*/

var obj = {};

Object.defineProperties(obj, {
  property: {
    configurable: 123
  }
});

var hadOwnProperty = obj.hasOwnProperty("property");

delete obj.property;

assert.sameValue(obj.hasOwnProperty("property"), false, 'obj.hasOwnProperty("property")');
assert(hadOwnProperty, 'hadOwnProperty !== true');
