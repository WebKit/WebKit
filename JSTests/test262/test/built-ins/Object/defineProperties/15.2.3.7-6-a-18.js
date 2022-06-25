// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es5id: 15.2.3.7-6-a-18
description: >
    Object.defineProperties - 'O' is a Date object which implements
    its own [[GetOwnProperty]] method to get 'P' (8.12.9 step 1 )
includes: [propertyHelper.js]
---*/


var obj = new Date();

Object.defineProperty(obj, "prop", {
  value: 11,
  configurable: false
});

try {
  Object.defineProperties(obj, {
    prop: {
      value: 12,
      configurable: true
    }
  });
  throw new Test262Error("Expected an exception.");
} catch (e) {
  verifyEqualTo(obj, "prop", 11);

  verifyNotWritable(obj, "prop");

  verifyNotEnumerable(obj, "prop");

  verifyNotConfigurable(obj, "prop");

  if (!(e instanceof TypeError)) {
    throw new Test262Error("Expected TypeError, got " + e);
  }

}
