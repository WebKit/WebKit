// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The production QuantifierPrefix :: + evaluates by returning the two
    results 1 and \infty
es5id: 15.10.2.7_A3_T7
description: >
    Execute /[a-z]+(\d+)/.exec("x 2 ff 55 x2 as1 z12 abc12.0") and
    check results
---*/

var __executed = /[a-z]+(\d+)/.exec("x 2 ff 55 x2 as1 z12 abc12.0");

var __expected = ["x2","2"];
__expected.index = 10;
__expected.input = "x 2 ff 55 x2 as1 z12 abc12.0";

//CHECK#1
if (__executed.length !== __expected.length) {
	throw new Test262Error('#1: __executed = /[a-z]+(\\d+)/.exec("x 2 ff 55 x2 as1 z12 abc12.0"); __executed.length === ' + __expected.length + '. Actual: ' + __executed.length);
}

//CHECK#2
if (__executed.index !== __expected.index) {
	throw new Test262Error('#2: __executed = /[a-z]+(\\d+)/.exec("x 2 ff 55 x2 as1 z12 abc12.0"); __executed.index === ' + __expected.index + '. Actual: ' + __executed.index);
}

//CHECK#3
if (__executed.input !== __expected.input) {
	throw new Test262Error('#3: __executed = /[a-z]+(\\d+)/.exec("x 2 ff 55 x2 as1 z12 abc12.0"); __executed.input === ' + __expected.input + '. Actual: ' + __executed.input);
}

//CHECK#4
for(var index=0; index<__expected.length; index++) {
	if (__executed[index] !== __expected[index]) {
		throw new Test262Error('#4: __executed = /[a-z]+(\\d+)/.exec("x 2 ff 55 x2 as1 z12 abc12.0"); __executed[' + index + '] === ' + __expected[index] + '. Actual: ' + __executed[index]);
	}
}
