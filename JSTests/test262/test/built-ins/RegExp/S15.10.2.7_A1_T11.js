// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The production QuantifierPrefix :: { DecimalDigits , DecimalDigits }
    evaluates as ...
es5id: 15.10.2.7_A1_T11
description: Execute /bx{0,93}c/.exec("aaabbbbcccddeeeefffff") and check results
---*/

var __executed = /bx{0,93}c/.exec("aaabbbbcccddeeeefffff");

var __expected = ["bc"];
__expected.index = 6;
__expected.input = "aaabbbbcccddeeeefffff";

//CHECK#1
if (__executed.length !== __expected.length) {
	throw new Test262Error('#1: __executed = /bx{0,93}c/.exec("aaabbbbcccddeeeefffff"); __executed.length === ' + __expected.length + '. Actual: ' + __executed.length);
}

//CHECK#2
if (__executed.index !== __expected.index) {
	throw new Test262Error('#2: __executed = /bx{0,93}c/.exec("aaabbbbcccddeeeefffff"); __executed.index === ' + __expected.index + '. Actual: ' + __executed.index);
}

//CHECK#3
if (__executed.input !== __expected.input) {
	throw new Test262Error('#3: __executed = /bx{0,93}c/.exec("aaabbbbcccddeeeefffff"); __executed.input === ' + __expected.input + '. Actual: ' + __executed.input);
}

//CHECK#4
for(var index=0; index<__expected.length; index++) {
	if (__executed[index] !== __expected[index]) {
		throw new Test262Error('#4: __executed = /bx{0,93}c/.exec("aaabbbbcccddeeeefffff"); __executed[' + index + '] === ' + __expected[index] + '. Actual: ' + __executed[index]);
	}
}
