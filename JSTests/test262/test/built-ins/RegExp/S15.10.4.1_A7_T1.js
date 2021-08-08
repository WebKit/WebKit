// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The [[Prototype]] property of the newly constructed object is set to the
    original RegExp prototype object, the one that is the initial value of
    RegExp.prototype
es5id: 15.10.4.1_A7_T1
description: >
    Add new property to [[Prototype]] of REgExp and check this
    property of the newly constructed object
---*/

var __re = new RegExp;
RegExp.prototype.indicator = 1;

//CHECK#1
if (__re.indicator !== 1) {
	throw new Test262Error('#1: __re = new RegExp; RegExp.prototype.indicator = 1; __re.indicator === 1. Actual: ' + (__re.indicator));
}
