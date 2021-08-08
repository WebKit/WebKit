// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The initial value of RegExp.prototype.constructor is the built-in RegExp
    constructor
es5id: 15.10.6.1_A1_T2
description: >
    Compare instance.constructor !== RegExp, where instance is new
    RegExp.prototype.constructor
---*/

var __FACTORY = RegExp.prototype.constructor;

var __instance = new __FACTORY;

//CHECK#1
if ((__instance instanceof RegExp) !== true) {
	throw new Test262Error('#1: __FACTORY = RegExp.prototype.constructor; __instance = new __FACTORY; (__instance instanceof RegExp) === true');
}

//CHECK#2
if (__instance.constructor !== RegExp) {
	throw new Test262Error('#2: __FACTORY = RegExp.prototype.constructor; __instance = new __FACTORY; __instance.constructor === RegExp. Actual: ' + (__instance.constructor));
}
