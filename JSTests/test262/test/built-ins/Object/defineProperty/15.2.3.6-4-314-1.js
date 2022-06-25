// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-314-1
description: >
    Object.defineProperty - 'O' is an Arguments object of a function
    that has formal parameters, 'P' is property, and 'desc' is
    accessor descriptor, test 'P' is defined in 'O' with all correct
    attribute values (10.6 [[DefineOwnProperty]] step 3)
includes: [propertyHelper.js]
---*/

(function(a, b, c) {
  function getFunc() {
    return "getFunctionString";
  }

  function setFunc(value) {
    this.testgetFunction = value;
  }
  Object.defineProperty(arguments, "genericProperty", {
    get: getFunc,
    set: setFunc,
    enumerable: true,
    configurable: true
  });
  verifyEqualTo(arguments, "genericProperty", getFunc());

  verifyWritable(arguments, "genericProperty", "testgetFunction");

  verifyEnumerable(arguments, "genericProperty");

  verifyConfigurable(arguments, "genericProperty");
}(1, 2, 3));
