// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: String.prototype.charCodeAt(pos)
es5id: 15.5.4.5_A1_T1
description: pos is false and true, and instance is object
---*/

var __instance = new Object(42);

__instance.charCodeAt = String.prototype.charCodeAt;

//////////////////////////////////////////////////////////////////////////////
//CHECK#1
if ((__instance.charCodeAt(false) !== 52) || (__instance.charCodeAt(true) !== 50)) {
  throw new Test262Error('#1: __instance = new Object(42); __instance.charCodeAt = String.prototype.charCodeAt;  __instance.charCodeAt(false) === 52 and __instance.charCodeAt(true) === 50. Actual: __instance.charCodeAt(false) ===' + __instance.charCodeAt(false) + ' and __instance.charCodeAt(true) ===' + __instance.charCodeAt(true));
}
//
//////////////////////////////////////////////////////////////////////////////
