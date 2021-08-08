// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    An escape sequence of the form \ followed by a nonzero decimal number n
    matches the result of the nth set of capturing parentheses (see
    15.10.2.11)
es5id: 15.10.2.9_A1_T1
description: >
    Execute /\b(\w+) \1\b/.exec("do you listen the the band") and
    check results
---*/

var __executed = /\b(\w+) \1\b/.exec("do you listen the the band");

var __expected = ["the the", "the"];
__expected.index = 14;
__expected.input = "do you listen the the band";

//CHECK#1
if (__executed.length !== __expected.length) {
	throw new Test262Error('#1: __executed = /\\b(\\w+) \\1\\b/.exec("do you listen the the band"); __executed.length === ' + __expected.length + '. Actual: ' + __executed.length);
}

//CHECK#2
if (__executed.index !== __expected.index) {
	throw new Test262Error('#2: __executed = /\\b(\\w+) \\1\\b/.exec("do you listen the the band"); __executed.index === ' + __expected.index + '. Actual: ' + __executed.index);
}

//CHECK#3
if (__executed.input !== __expected.input) {
	throw new Test262Error('#3: __executed = /\\b(\\w+) \\1\\b/.exec("do you listen the the band"); __executed.input === ' + __expected.input + '. Actual: ' + __executed.input);
}

//CHECK#4
for(var index=0; index<__expected.length; index++) {
	if (__executed[index] !== __expected[index]) {
		throw new Test262Error('#4: __executed = /\\b(\\w+) \\1\\b/.exec("do you listen the the band"); __executed[' + index + '] === ' + __expected[index] + '. Actual: ' + __executed[index]);
	}
}
