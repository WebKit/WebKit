// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    In case-insignificant matches all characters are implicitly converted to
    upper case immediately before they are compared
es5id: 15.10.2.8_A5_T1
description: Execute /[a-z]+/ig.exec("ABC def ghi") and check results
---*/

var __string = "ABC def ghi";
var __executed = /[a-z]+/ig.exec(__string);

var __expected = ["ABC"];
__expected.index = 0;
__expected.input = __string;

//CHECK#1
if (__executed.length !== __expected.length) {
	throw new Test262Error('#1: __string = "ABC def ghi"; __executed = /[a-z]+/ig.exec(__string); __executed.length === ' + __expected.length + '. Actual: ' + __executed.length);
}

//CHECK#2
if (__executed.index !== __expected.index) {
	throw new Test262Error('#2: __string = "ABC def ghi"; __executed = /[a-z]+/ig.exec(__string); __executed.index === ' + __expected.index + '. Actual: ' + __executed.index);
}

//CHECK#3
if (__executed.input !== __expected.input) {
	throw new Test262Error('#3: __string = "ABC def ghi"; __executed = /[a-z]+/ig.exec(__string); __executed.input === ' + __expected.input + '. Actual: ' + __executed.input);
}

//CHECK#4
for(var index=0; index<__expected.length; index++) {
	if (__executed[index] !== __expected[index]) {
		throw new Test262Error('#4: __string = "ABC def ghi"; __executed = /[a-z]+/ig.exec(__string); __executed[' + index + '] === ' + __expected[index] + '. Actual: ' + __executed[index]);
	}
}
