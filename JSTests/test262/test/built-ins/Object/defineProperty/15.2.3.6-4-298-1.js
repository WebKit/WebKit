// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-298-1
description: >
    Object.defineProperty - 'O' is an Arguments object of a function
    that has formal parameters, 'name' is own accessor property of 'O'
    which is also defined in [[ParameterMap]] of 'O', test TypeError
    is thrown when updating the [[Set]] attribute value of 'name'
    which is defined as non-configurable (10.6 [[DefineOwnProperty]]
    steps 4 and 5a)
includes: [propertyHelper.js]
---*/

(function(a, b, c) {
  function getFunc() {
    return 10;
  }
  Object.defineProperty(arguments, "0", {
    get: getFunc,
    set: undefined,
    enumerable: false,
    configurable: false
  });

  function setFunc(value) {
    this.setVerifyHelpProp = value;
  }
  try {
    Object.defineProperty(arguments, "0", {
      set: setFunc
    });
    throw new Test262Error("Expected an exception.");
  } catch (e) {
    if (a !== 0) {
      throw new Test262Error('Expected a === 0, actually ' + a);
    }

    verifyEqualTo(arguments, "0", getFunc());

    verifyNotEnumerable(arguments, "0");

    verifyNotConfigurable(arguments, "0");

    if (!(e instanceof TypeError)) {
      throw new Test262Error("Expected TypeError, got " + e);
    }

  }
}(0, 1, 2));
