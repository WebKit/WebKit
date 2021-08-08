// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The production QuantifierPrefix :: * evaluates by returning the two
    results 0 and \infty
es5id: 15.10.2.7_A4_T19
description: Execute /bc..[\d]*[\s]* /.exec('abcdef') and check results
---*/

var __executed = /bc..[\d]*[\s]*/.exec('abcdef');

var __expected = ["bcde"];
__expected.index = 1;
__expected.input = 'abcdef';

//CHECK#1
if (__executed.length !== __expected.length) {
	throw new Test262Error('#1: __executed = /bc..[\\d]*[\\s]*/.exec(\'abcdef\'); __executed.length === ' + __expected.length + '. Actual: ' + __executed.length);
}

//CHECK#2
if (__executed.index !== __expected.index) {
	throw new Test262Error('#2: __executed = /bc..[\\d]*[\\s]*/.exec(\'abcdef\'); __executed.index === ' + __expected.index + '. Actual: ' + __executed.index);
}

//CHECK#3
if (__executed.input !== __expected.input) {
	throw new Test262Error('#3: __executed = /bc..[\\d]*[\\s]*/.exec(\'abcdef\'); __executed.input === ' + __expected.input + '. Actual: ' + __executed.input);
}

//CHECK#4
for(var index=0; index<__expected.length; index++) {
	if (__executed[index] !== __expected[index]) {
		throw new Test262Error('#4: __executed = /bc..[\\d]*[\\s]*/.exec(\'abcdef\'); __executed[' + index + '] === ' + __expected[index] + '. Actual: ' + __executed[index]);
	}
}
