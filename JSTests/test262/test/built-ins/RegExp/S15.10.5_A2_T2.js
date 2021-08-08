// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The value of the internal [[Prototype]] property of the RegExp
    constructor is the Function prototype object
es5id: 15.10.5_A2_T2
description: >
    Add new property to Function.prototype and then check this
    property of RegExp
---*/

Function.prototype.indicator = 1;

 //CHECK#1
if (RegExp.indicator !== 1) {
	throw new Test262Error('#1: Function.prototype.indicator = 1; RegExp.indicator === 1. Actual: ' + (RegExp.indicator));
}
