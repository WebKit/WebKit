// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The production QuantifierPrefix :: ? evaluates by returning the two
    results 0 and 1
es5id: 15.10.2.7_A5_T7
description: Execute /x?y?z?/.exec("abcd") and check results
---*/

var __executed = /x?y?z?/.exec("abcd");

var __expected = [""];
__expected.index = 0;
__expected.input = "abcd";

//CHECK#1
if (__executed.length !== __expected.length) {
	throw new Test262Error('#1: __executed = /x?y?z?/.exec("abcd"); __executed.length === ' + __expected.length + '. Actual: ' + __executed.length);
}

//CHECK#2
if (__executed.index !== __expected.index) {
	throw new Test262Error('#2: __executed = /x?y?z?/.exec("abcd"); __executed.index === ' + __expected.index + '. Actual: ' + __executed.index);
}

//CHECK#3
if (__executed.input !== __expected.input) {
	throw new Test262Error('#3: __executed = /x?y?z?/.exec("abcd"); __executed.input === ' + __expected.input + '. Actual: ' + __executed.input);
}

//CHECK#4
for(var index=0; index<__expected.length; index++) {
	if (__executed[index] !== __expected[index]) {
		throw new Test262Error('#4: __executed = /x?y?z?/.exec("abcd"); __executed[' + index + '] === ' + __expected[index] + '. Actual: ' + __executed[index]);
	}
}
