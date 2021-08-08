// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-538-6
description: >
    ES5 Attributes - Updating an indexed accessor property 'P' whose
    [[Configurable]] attribute is true to a data property is
    successful, 'O' is an Arguments object
includes: [propertyHelper.js]
---*/

var obj = (function() {
  return arguments;
}());

obj.verifySetFunc = "data";
var getFunc = function() {
  return obj.verifySetFunc;
};

var setFunc = function(value) {
  obj.verifySetFunc = value;
};

Object.defineProperty(obj, "0", {
  get: getFunc,
  set: setFunc,
  enumerable: true,
  configurable: true
});
var desc1 = Object.getOwnPropertyDescriptor(obj, "0");

Object.defineProperty(obj, "0", {
  value: 1001
});
var desc2 = Object.getOwnPropertyDescriptor(obj, "0");

if (!desc1.hasOwnProperty("get")) {
  throw new Test262Error('Expected desc1.hasOwnProperty("get") to be true, actually ' + desc1.hasOwnProperty("get"));
}

if (!desc2.hasOwnProperty("value")) {
  throw new Test262Error('Expected desc2.hasOwnProperty("value") to be true, actually ' + desc2.hasOwnProperty("value"));
}

if (typeof desc2.get !== "undefined") {
  throw new Test262Error('Expected typeof desc2.get === "undefined" , actually ' + typeof desc2.get);
}

if (typeof desc2.set !== "undefined") {
  throw new Test262Error('Expected typeof desc2.set === "undefined" , actually ' + typeof desc2.set);
}

verifyEqualTo(obj, "0", 1001);

verifyNotWritable(obj, "0");

verifyEnumerable(obj, "0");

verifyConfigurable(obj, "0");
