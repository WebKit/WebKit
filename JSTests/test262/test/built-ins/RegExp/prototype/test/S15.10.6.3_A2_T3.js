// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    A TypeError exception is thrown if the this value is not an object for
    which the value of the internal [[Class]] property is "RegExp"
es5id: 15.10.6.3_A2_T3
description: The tested object is function object
---*/

__instance.test = RegExp.prototype.test;

//CHECK#1
try {
  __instance.test("message to investigate");
  throw new Test262Error('#1.1: __instance.test = RegExp.prototype.test; __instance.test("message to investigate"); function __instance(){}');
} catch (e) {
  if ((e instanceof TypeError) !== true) {
    throw new Test262Error('#1.2: __instance.test = RegExp.prototype.test; __instance.test("message to investigate"); function __instance(){}. Actual: ' + (e));
  }
}

function __instance(){};
