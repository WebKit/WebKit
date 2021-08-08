// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    RegExp.prototype.exec(string) Performs a regular expression match of ToString(string) against the regular expression and
    returns an Array object containing the results of the match, or null if the string did not match
es5id: 15.10.6.2_A1_T10
description: String is 1.01 and RegExp is /1|12/
---*/

var __executed = /1|12/.exec(1.01);

var __expected = ["1"];
__expected.index=0;
__expected.input="1.01";

//CHECK#0
if ((__executed instanceof Array) !== true) {
	throw new Test262Error('#0: __executed = /1|12/.exec(1.01); (__executed instanceof Array) === true');
}

//CHECK#1
if (__executed.length !== __expected.length) {
  throw new Test262Error('#1: __executed = /1|12/.exec(1.01); __executed.length === ' + __expected.length + '. Actual: ' + __executed.length);
}

//CHECK#2
if (__executed.index !== __expected.index) {
  throw new Test262Error('#2: __executed = /1|12/.exec(1.01); __executed.index === ' + __expected.index + '. Actual: ' + __executed.index);
}

//CHECK#3
if (__executed.input !== __expected.input) {
  throw new Test262Error('#3: __executed = /1|12/.exec(1.01); __executed.input === ' + __expected.input + '. Actual: ' + __executed.input);
}

//CHECK#4
for(var index=0; index<__expected.length; index++) {
  if (__executed[index] !== __expected[index]) {
    throw new Test262Error('#4: __executed = /1|12/.exec(1.01); __executed[' + index + '] === ' + __expected[index] + '. Actual: ' + __executed[index]);
  }
}
