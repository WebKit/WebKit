// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Equivalent to the expression RegExp.prototype.exec(string) != null
es5id: 15.10.6.3_A1_T9
description: RegExp is /1|12/ and tested string is function object
---*/

var __string;
var __re = /1|12/;

//CHECK#0
if (__re.test(__string) !== (__re.exec(__string) !== null)) {
	throw new Test262Error('#0: var __string;__re = /1|12/; __re.test(__string) === (__re.exec(__string) !== null); function __string(){};');
}

function __string(){};
