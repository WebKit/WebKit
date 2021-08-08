// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    A TypeError exception is thrown if the this value is not an object for
    which the value of the internal [[Class]] property is "RegExp"
es5id: 15.10.6.2_A2_T1
description: The internal [[Class]] property is "Object"
---*/

var __instance = new Object;

__instance.exec = RegExp.prototype.exec;

//CHECK#1
try {
	throw new Test262Error('#1.1: __instance = new Object; __instance.exec = RegExp.prototype.exec; __instance.exec("message to investigate"). Actual: ' + (__instance.exec("message to investigate")));
} catch (e) {
	if ((e instanceof TypeError) !== true) {
		throw new Test262Error('#1.2: __instance = new Object; __instance.exec = RegExp.prototype.exec. Actual: ' + (e));
	}
}
