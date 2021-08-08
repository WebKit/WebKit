// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    Number instances have no special properties beyond those
    inherited from the Number prototype object
es5id: 15.7.5_A1_T02
description: Checking property toString
---*/

//CHECK#1
if ((new Number()).hasOwnProperty("toString") !== false) {
  throw new Test262Error('#1: Number instance must have no special property "toString"');
}

//CHECK#2
if ((new Number()).toString !== Number.prototype.toString) {
  throw new Test262Error('#2: Number instance property "toString" must be inherited from Number prototype object');
}
