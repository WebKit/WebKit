// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: RegExp instance type is RegExp
es5id: 15.10.7_A3_T1
description: >
    Checking type of RegExp instance with operators typeof, instanceof
    and check it constructor.  RegExp instance is /[^a]* /
---*/

var __re = /[^a]*/;

//CHECK#1
if (typeof __re !== "object") {
	throw new Test262Error('#1: __re = /[^a]*/; typeof __re === "object". Actual: ' + (typeof __re));
}

//CHECK#1
if (__re.constructor !== RegExp) {
	throw new Test262Error('#2: __re = /[^a]*/; __re.constructor === RegExp. Actual: ' + (__re.constructor));
}

//CHECK#3
if ((__re instanceof RegExp) !== true) {
	throw new Test262Error('#3: __re = /[^a]*/; (__re instanceof RegExp) === true');
}
