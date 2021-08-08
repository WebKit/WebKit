// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.7-6-a-244
description: >
    Object.defineProperties - TypeError is thrown if 'O' is an Array,
    'P' is an array index named property that already exists on 'O' is
    accessor property with  [[Configurable]] false, 'desc' is accessor
    descriptor, the [[Get]] field of 'desc' is present, and the
    [[Get]] field of 'desc' and the [[Get]] attribute value of 'P' are
    two objects which refer to the different objects  (15.4.5.1 step
    4.c)
includes: [propertyHelper.js]
---*/

var arr = [];

function get_fun() {
  return 36;
}
Object.defineProperty(arr, "1", {
  get: get_fun
});

try {
  Object.defineProperties(arr, {
    "1": {
      get: function() {
        return 12;
      }
    }
  });

  throw new Test262Error("Expected an exception.");
} catch (e) {
  verifyEqualTo(arr, "1", get_fun());

  verifyNotEnumerable(arr, "1");

  verifyNotConfigurable(arr, "1");

  if (!(e instanceof TypeError)) {
    throw new Test262Error("Expected TypeError, got " + e);
  }

}
