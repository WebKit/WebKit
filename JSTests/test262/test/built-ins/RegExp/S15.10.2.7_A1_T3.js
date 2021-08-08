// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The production QuantifierPrefix :: { DecimalDigits , DecimalDigits }
    evaluates as ...
es5id: 15.10.2.7_A1_T3
description: >
    Execute /\d{2,4}/.exec("the 20000 Leagues Under the Sea book") and
    check results
---*/

var __executed = /\d{2,4}/.exec("the 20000 Leagues Under the Sea book");

var __expected = ["2000"];
__expected.index = 4;
__expected.input = "the 20000 Leagues Under the Sea book";

//CHECK#1
if (__executed.length !== __expected.length) {
	throw new Test262Error('#1: __executed = /\\d{2,4}/.exec("the 20000 Leagues Under the Sea book"); __executed.length === ' + __expected.length + '. Actual: ' + __executed.length);
}

//CHECK#2
if (__executed.index !== __expected.index) {
	throw new Test262Error('#2: __executed = /\\d{2,4}/.exec("the 20000 Leagues Under the Sea book"); __executed.index === ' + __expected.index + '. Actual: ' + __executed.index);
}

//CHECK#3
if (__executed.input !== __expected.input) {
	throw new Test262Error('#3: __executed = /\\d{2,4}/.exec("the 20000 Leagues Under the Sea book"); __executed.input === ' + __expected.input + '. Actual: ' + __executed.input);
}

//CHECK#4
for(var index=0; index<__expected.length; index++) {
	if (__executed[index] !== __expected[index]) {
		throw new Test262Error('#4: __executed = /\\d{2,4}/.exec("the 20000 Leagues Under the Sea book"); __executed[' + index + '] === ' + __expected[index] + '. Actual: ' + __executed[index]);
	}
}
