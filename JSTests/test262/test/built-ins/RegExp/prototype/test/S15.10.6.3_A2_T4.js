// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    A TypeError exception is thrown if the this value is not an object for
    which the value of the internal [[Class]] property is "RegExp"
es5id: 15.10.6.3_A2_T4
description: The tested object is new String("[a-b]")
---*/

var __instance = new String("[a-b]");

__instance.test = RegExp.prototype.test;

//CHECK#1
    try {
      throw new Test262Error('#1.1: __instance = new String("[a-b]"); __instance.test = RegExp.prototype.test; test("message to investigate"). Actual: ' + (__instance.test("message to investigate")));
    } catch (e) {
      if ((e instanceof TypeError) !== true) {
        throw new Test262Error('#1.2: __instance = new String("[a-b]"); __instance.test = RegExp.prototype.test; test("message to investigate"). Actual: ' + (e));
      }
   }
