// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The production QuantifierPrefix :: * evaluates by returning the two
    results 0 and \infty
es5id: 15.10.2.7_A4_T16
description: Execute /(x+)(x*)/.exec('xxxxxxx') and check results
---*/

var __executed = /(x+)(x*)/.exec('xxxxxxx');

var __expected = ["xxxxxxx","xxxxxxx",""];
__expected.index = 0;
__expected.input = 'xxxxxxx';

//CHECK#1
if (__executed.length !== __expected.length) {
	throw new Test262Error('#1: __executed = /(x+)(x*)/.exec(\'xxxxxxx\'); __executed.length === ' + __expected.length + '. Actual: ' + __executed.length);
}

//CHECK#2
if (__executed.index !== __expected.index) {
	throw new Test262Error('#2: __executed = /(x+)(x*)/.exec(\'xxxxxxx\'); __executed.index === ' + __expected.index + '. Actual: ' + __executed.index);
}

//CHECK#3
if (__executed.input !== __expected.input) {
	throw new Test262Error('#3: __executed = /(x+)(x*)/.exec(\'xxxxxxx\'); __executed.input === ' + __expected.input + '. Actual: ' + __executed.input);
}

//CHECK#4
for(var index=0; index<__expected.length; index++) {
	if (__executed[index] !== __expected[index]) {
		throw new Test262Error('#4: __executed = /(x+)(x*)/.exec(\'xxxxxxx\'); __executed[' + index + '] === ' + __expected[index] + '. Actual: ' + __executed[index]);
	}
}
