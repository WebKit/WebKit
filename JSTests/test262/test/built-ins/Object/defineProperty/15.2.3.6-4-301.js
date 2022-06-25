// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.6-4-301
description: >
    Object.defineProperty - 'O' is an Arguments object, 'name' is an
    array index named property of 'O' but not defined in
    [[ParameterMap]] of 'O', and 'desc' is data descriptor, test
    'name' is defined in 'O' with all correct attribute values (10.6
    [[DefineOwnProperty]] step 3)
includes: [propertyHelper.js]
---*/

(function() {
  delete arguments[0];
  Object.defineProperty(arguments, "0", {
    value: 10,
    writable: false,
    enumerable: false,
    configurable: false
  });
  verifyEqualTo(arguments, "0", 10);

  verifyNotWritable(arguments, "0");

  verifyNotEnumerable(arguments, "0");

  verifyNotConfigurable(arguments, "0");
}(0, 1, 2));
