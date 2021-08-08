// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    String.prototype.substring (start, end) can be applied to non String object instance and
    returns a string value(not object)
es5id: 15.5.4.15_A3_T9
description: >
    Apply String.prototype.substring to Math instance. Start is
    Math.PI, end is -10
---*/

var __instance = Math;

__instance.substring = String.prototype.substring;

//////////////////////////////////////////////////////////////////////////////
//CHECK#1
if (__instance.substring(Math.PI, -10) !== "[ob") {
  throw new Test262Error('#1: __instance = Math; __instance.substring = String.prototype.substring;  __instance.substring(Math.PI, -10) === "[ob". Actual: ' + __instance.substring(Math.PI, -10));
}
//
//////////////////////////////////////////////////////////////////////////////
