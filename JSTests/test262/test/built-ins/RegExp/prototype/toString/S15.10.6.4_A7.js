// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: RegExp.prototype.toString can't be used as constructor
es5id: 15.10.6.4_A7
description: Checking if creating the RegExp.prototype.toString object fails
---*/

var __FACTORY = RegExp.prototype.toString;

try {
    var __instance = new __FACTORY;
    throw new Test262Error('#1.1: __FACTORY = RegExp.prototype.toString throw TypeError. Actual: ' + (__instance));
} catch (e) {
  assert.sameValue(
    e instanceof TypeError,
    true,
    'The result of evaluating (e instanceof TypeError) is expected to be true'
  );
}

// TODO: Convert to assert.throws() format.
