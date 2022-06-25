// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    A TypeError exception is thrown if the this value is not an object for
    which the value of the internal [[Class]] property is "RegExp"
es5id: 15.10.6.3_A2_T8
description: The tested object is "."
---*/

var __instance = ".";

Object.prototype.test = RegExp.prototype.test;

try {
  throw new Test262Error('#1.1: __instance = "."; Object.prototype.test = RegExp.prototype.test; __instance.test("message to investigate"). Actual: ' + (__instance.test("message to investigate")));
} catch (e) {
  assert.sameValue(
    e instanceof TypeError,
    true,
    'The result of evaluating (e instanceof TypeError) is expected to be true'
  );
}

// TODO: Convert to assert.throws() format.
