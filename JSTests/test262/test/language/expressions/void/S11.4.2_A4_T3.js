// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Operator "void" evaluates UnaryExpression and returns undefined
es5id: 11.4.2_A4_T3
description: Type(x) is string primitive of String object
---*/

//CHECK#1
var x = "1";
if (void x !== undefined) {
  throw new Test262Error('#1: var x = "1"; void x === undefined. Actual: ' + (void x));
}

//CHECK#2
var x = "x"; 
if (isNaN(void x) !== true) {
  throw new Test262Error('#2: var x = "x"; void x === undefined. Actual: ' + (void x));
}

//CHECK#3
var x = new String("-1");
if (void x !== undefined) {
  throw new Test262Error('#3: var x = new String("-1"); void x === undefined. Actual: ' + (void x));
}
