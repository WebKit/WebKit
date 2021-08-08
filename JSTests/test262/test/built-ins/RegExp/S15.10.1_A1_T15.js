// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: RegExp syntax errors must be caught when matcher(s) compiles
es5id: 15.10.1_A1_T15
description: Tested RegExp is "x{1,}{1}"
---*/

//CHECK#1
try {
	throw new Test262Error('#1.1: new RegExp("x{1,}{1}") throw SyntaxError. Actual: ' + (new RegExp("x{1,}{1}")));
} catch (e) {
	if ((e instanceof SyntaxError) !== true) {
		throw new Test262Error('#1.2: new RegExp("x{1,}{1}") throw SyntaxError. Actual: ' + (e));
	}
}
