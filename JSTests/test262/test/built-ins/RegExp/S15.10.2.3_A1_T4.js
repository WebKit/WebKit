// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The | regular expression operator separates two alternatives.
    The pattern first tries to match the left Alternative (followed by the sequel of the regular expression).
    If it fails, it tries to match the right Disjunction (followed by the sequel of the regular expression)
es5id: 15.10.2.3_A1_T4
description: >
    Execute /\d{3}|[a-z]{4}/.exec("2, 12 and 234 AND of course repeat
    12") and check results
---*/

var __executed = /\d{3}|[a-z]{4}/.exec("2, 12 and 234 AND of course repeat 12");

var __expected = ["234"];
__expected.index = 10;
__expected.input = "2, 12 and 234 AND of course repeat 12";

//CHECK#1
if (__executed.length !== __expected.length) {
	throw new Test262Error('#1: __executed = /\\d{3}|[a-z]{4}/.exec("2, 12 and 234 AND of course repeat 12"); __executed.length === ' + __expected.length + '. Actual: ' + __executed.length);
}

//CHECK#2
if (__executed.index !== __expected.index) {
	throw new Test262Error('#2: __executed = /\\d{3}|[a-z]{4}/.exec("2, 12 and 234 AND of course repeat 12"); __executed.index === ' + __expected.index + '. Actual: ' + __executed.index);
}

//CHECK#3
if (__executed.input !== __expected.input) {
	throw new Test262Error('#3: __executed = /\\d{3}|[a-z]{4}/.exec("2, 12 and 234 AND of course repeat 12"); __executed.input === ' + __expected.input + '. Actual: ' + __executed.input);
}

//CHECK#4
for(var index=0; index<__expected.length; index++) {
	if (__executed[index] !== __expected[index]) {
		throw new Test262Error('#4: __executed = /\\d{3}|[a-z]{4}/.exec("2, 12 and 234 AND of course repeat 12"); __executed[' + index + '] === ' + __expected[index] + '. Actual: ' + __executed[index]);
	}
}
