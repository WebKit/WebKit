// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The initial value of RegExp.prototype.constructor is the built-in RegExp
    constructor
es5id: 15.10.6.1_A1_T1
description: Compare RegExp.prototype.constructor with RegExp
---*/

//CHECK#1
if (RegExp.prototype.constructor !== RegExp) {
	throw new Test262Error('#1: RegExp.prototype.constructor === RegExp. Actual: ' + (RegExp.prototype.constructor));
}
