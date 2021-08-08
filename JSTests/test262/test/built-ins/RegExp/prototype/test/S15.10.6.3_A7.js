// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: RegExp.prototype.test can't be used as constructor
es5id: 15.10.6.3_A7
description: Checking if creating the RegExp.prototype.test object fails
---*/

var __FACTORY = RegExp.prototype.test;

try {
  var __instance = new __FACTORY;
  throw new Test262Error('#1.1: __FACTORY = RegExp.prototype.test throw TypeError. Actual: ' + (__instance));
} catch (e) {
  if ((e instanceof TypeError) !== true) {
    throw new Test262Error('#1.2: __FACTORY = RegExp.prototype.test throw TypeError. Actual: ' + (e));
  }
}
