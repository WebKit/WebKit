// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The form (?= Disjunction ) specifies a zero-width positive lookahead.
    In order for it to succeed, the pattern inside Disjunction must match at the current position, but the current position is not advanced before matching the sequel.
    If Disjunction can match at the current position in several ways, only the first one is tried
es5id: 15.10.2.8_A1_T4
description: >
    Execute /[Jj]ava([Ss]cript)?(?=\:)/.exec("taste of java: the
    cookbook ") and check results
---*/

var __executed = /[Jj]ava([Ss]cript)?(?=\:)/.exec("taste of java: the cookbook ");

var __expected = ["java", undefined];
__expected.index = 9;
__expected.input = "taste of java: the cookbook ";

//CHECK#1
if (__executed.length !== __expected.length) {
	throw new Test262Error('#1: __executed = /[Jj]ava([Ss]cript)?(?=\\:)/.exec("taste of java: the cookbook "); __executed.length === ' + __expected.length + '. Actual: ' + __executed.length);
}

//CHECK#2
if (__executed.index !== __expected.index) {
	throw new Test262Error('#2: __executed = /[Jj]ava([Ss]cript)?(?=\\:)/.exec("taste of java: the cookbook "); __executed.index === ' + __expected.index + '. Actual: ' + __executed.index);
}

//CHECK#3
if (__executed.input !== __expected.input) {
	throw new Test262Error('#3: __executed = /[Jj]ava([Ss]cript)?(?=\\:)/.exec("taste of java: the cookbook "); __executed.input === ' + __expected.input + '. Actual: ' + __executed.input);
}

//CHECK#4
for(var index=0; index<__expected.length; index++) {
	if (__executed[index] !== __expected[index]) {
		throw new Test262Error('#4: __executed = /[Jj]ava([Ss]cript)?(?=\\:)/.exec("taste of java: the cookbook "); __executed[' + index + '] === ' + __expected[index] + '. Actual: ' + __executed[index]);
	}
}
