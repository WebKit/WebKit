// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.7-6-a-282
description: >
    Object.defineProperties - 'O' is an Arguments object, 'P' is own
    data property of 'O' which is also defined in [[ParameterMap]] of
    'O', test TypeError is thrown when updating the [[Value]]
    attribute value of 'P' whose writable and configurable attributes
    are false (10.6 [[DefineOwnProperty]] step 4)
includes: [propertyHelper.js]
---*/


var arg;

(function fun(a, b, c) {
  arg = arguments;
}(0, 1, 2));

Object.defineProperty(arg, "0", {
  value: 0,
  writable: false,
  configurable: false
});

try {
  Object.defineProperties(arg, {
    "0": {
      value: 10
    }
  });

  throw new Test262Error("Expected an exception.");
} catch (e) {
  verifyEqualTo(arg, "0", 0);

  verifyNotWritable(arg, "0");

  verifyEnumerable(arg, "0");

  verifyNotConfigurable(arg, "0");

  if (!(e instanceof TypeError)) {
    throw new Test262Error("Expected TypeError, got " + e);
  }

}
