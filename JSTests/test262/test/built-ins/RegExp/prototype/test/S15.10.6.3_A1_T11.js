// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Equivalent to the expression RegExp.prototype.exec(string) != null
es5id: 15.10.6.3_A1_T11
description: RegExp is /2|12/ and tested string is new Number(1.012)
---*/

var __string = new Number(1.012);
var __re = /2|12/;

//CHECK#0
if (__re.test(__string) !== (__re.exec(__string) !== null)) {
	throw new Test262Error('#0: var __string = new Number(1.012); __re = /2|12/; __re.test(__string) === (__re.exec(__string) !== null)');
}
