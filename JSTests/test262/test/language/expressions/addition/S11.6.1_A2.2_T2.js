// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Operator x + y uses [[Default Value]]
es5id: 11.6.1_A2.2_T2
description: If Type(value) is Date object, evaluate ToPrimitive(value, String)
---*/

//CHECK#1
var date = new Date();
if (date + date !== date.toString() + date.toString()) {
  throw new Test262Error('#1: var date = new Date(); date + date === date.toString() + date.toString(). Actual: ' + (date + date));  
}

//CHECK#2
var date = new Date();
if (date + 0 !== date.toString() + "0") {
  throw new Test262Error('#2: var date = new Date(); date + 0 === date.toString() + "0". Actual: ' + (date + 0));  
}

//CHECK#3
var date = new Date();
if (date + true !== date.toString() + "true") {
  throw new Test262Error('#3: var date = new Date(); date + true === date.toString() + "true". Actual: ' + (date + true));  
}

//CHECK#4
var date = new Date();
if (date + new Object() !== date.toString() + "[object Object]") {
  throw new Test262Error('#4: var date = new Date(); date + new Object() === date.toString() + "[object Object]". Actual: ' + (date + new Object()));  
}
