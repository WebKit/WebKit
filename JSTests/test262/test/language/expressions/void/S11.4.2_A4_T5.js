// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Operator "void" evaluates UnaryExpression and returns undefined
es5id: 11.4.2_A4_T5
description: Type(x) is Object object or Function object
---*/

//CHECK#1
var x = {}; 
if (isNaN(void x) !== true) {
  throw new Test262Error('#1: var x = {}; void x === undefined. Actual: ' + (void x));
}

//CHECK#2
var x = function(){return 1}; 
if (isNaN(void x) !== true) {
  throw new Test262Error('#2: var x = function(){return 1}; void x === undefined. Actual: ' + (void x));
}
