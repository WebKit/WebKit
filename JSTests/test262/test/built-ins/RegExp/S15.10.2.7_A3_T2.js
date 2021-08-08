// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The production QuantifierPrefix :: + evaluates by returning the two
    results 1 and \infty
es5id: 15.10.2.7_A3_T2
description: Execute /\s+java\s+/.exec("\t java object") and check results
---*/

var __executed = /\s+java\s+/.exec("\t java object");

var __expected = ["\t java "];
__expected.index = 0;
__expected.input = "\t java object";

//CHECK#1
if (__executed.length !== __expected.length) {
	throw new Test262Error('#1: __executed = /\\s+java\\s+/.exec("\\t java object"); __executed.length === ' + __expected.length + '. Actual: ' + __executed.length);
}

//CHECK#2
if (__executed.index !== __expected.index) {
	throw new Test262Error('#2: __executed = /\\s+java\\s+/.exec("\\t java object"); __executed.index === ' + __expected.index + '. Actual: ' + __executed.index);
}

//CHECK#3
if (__executed.input !== __expected.input) {
	throw new Test262Error('#3: __executed = /\\s+java\\s+/.exec("\\t java object"); __executed.input === ' + __expected.input + '. Actual: ' + __executed.input);
}

//CHECK#4
for(var index=0; index<__expected.length; index++) {
	if (__executed[index] !== __expected[index]) {
		throw new Test262Error('#4: __executed = /\\s+java\\s+/.exec("\\t java object"); __executed[' + index + '] === ' + __expected[index] + '. Actual: ' + __executed[index]);
	}
}
