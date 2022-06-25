// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.7-5-b-95
description: >
    Object.defineProperties - value of 'configurable' property of
    'descObj' is empty string (8.10.5 step 4.b)
includes: [propertyHelper.js]
---*/

var obj = {};

Object.defineProperties(obj, {
  property: {
    configurable: ""
  }
});

assert(obj.hasOwnProperty("property"));
verifyNotConfigurable(obj, "property");
