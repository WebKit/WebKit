// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    Number instances have no special properties beyond those
    inherited from the Number prototype object
es5id: 15.7.5_A1_T07
description: Checking property toPrecision
---*/

//CHECK#1
if ((new Number()).hasOwnProperty("toPrecision") !== false) {
  throw new Test262Error('#1: Number instance must have no special property "toPrecision"');
}

//CHECK#2
if ((new Number()).toPrecision !== Number.prototype.toPrecision) {
  throw new Test262Error('#2: Number instance property "toPrecision" must be inherited from Number prototype object');
}
