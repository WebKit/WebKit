// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Operator x++ uses GetValue and PutValue
es5id: 11.3.1_A2.1_T1
description: Type(x) is Reference and GetBase(x) is not null
---*/

//CHECK#1
var x = 1;
var y = x++;
if (y !== 1) {
  throw new Test262Error('#1: var x = 1; var y = x++; y === 1. Actual: ' + (y));
} else {
  if (x !== 1 + 1) {
    throw new Test262Error('#1: var x = 1; var y = x++; x === 1 + 1. Actual: ' + (x));
  } 
}

//CHECK#2
this.x = 1;
var y = this.x++; 
if (y !== 1) {
  throw new Test262Error('#2: this.x = 1; var y = this.x++; y === 1. Actual: ' + (y));
} else {
  if (this.x !== 1 + 1) {
    throw new Test262Error('#2: this.x = 1; var y = this.x++; this.x === 1 + 1. Actual: ' + (this.x));
  } 
}

//CHECK#3
var object = new Object();
object.prop = 1;
var y = object.prop++;
if (y !== 1) {
  throw new Test262Error('#3: var object = new Object(); object.prop = 1; var y = object.prop++; y === 1. Actual: ' + (y));
} else {
  if (this.x !== 1 + 1) {
    throw new Test262Error('#3: var object = new Object(); object.prop = 1; var y = object.prop++; object.prop === 1 + 1. Actual: ' + (object.prop));
  } 
}
