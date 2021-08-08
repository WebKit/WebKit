// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    Parentheses of the form ( Disjunction ) serve both to group the components of the Disjunction pattern together and to save the result of the match.
    The result can be used either in a backreference (\ followed by a nonzero decimal number),
    referenced in a replace string,
    or returned as part of an array from the regular expression matching function
es5id: 15.10.2.8_A3_T15
description: "see bug http:bugzilla.mozilla.org/show_bug.cgi?id=119909"
---*/

var __strOriginal = "hello";
var __openParen = '(';
var __closeParen = ')';
var __pattern = '';
var numParens = 200;
  
for (var i=0; i<numParens; i++)
    __pattern += __openParen;
    
__pattern += __strOriginal;

for (i=0; i<numParens; i++) 
    __pattern += __closeParen;
    
var __re = new RegExp(__pattern);

var __executed = __re.exec(__strOriginal);

var __expected = [];
for (var i=0; i<=numParens; i++)
    __expected.push(__strOriginal);
__expected.index = 0;
__expected.input = __strOriginal;

//CHECK#1
if (__executed.length !== __expected.length) {
	throw new Test262Error('#1: __re = new RegExp(__pattern); __executed = __re.exec(__strOriginal); __executed.length === ' + __expected.length + '. Actual: ' + __executed.length);
}

//CHECK#2
if (__executed.index !== __expected.index) {
	throw new Test262Error('#2: __re = new RegExp(__pattern); __executed = __re.exec(__strOriginal); __executed.index === ' + __expected.index + '. Actual: ' + __executed.index);
}

//CHECK#3
if (__executed.input !== __expected.input) {
	throw new Test262Error('#3: __re = new RegExp(__pattern); __executed = __re.exec(__strOriginal); __executed.input === ' + __expected.input + '. Actual: ' + __executed.input);
}

//CHECK#4
for(var index=0; index<__expected.length; index++) {
	if (__executed[index] !== __expected[index]) {
		throw new Test262Error('#4: __re = new RegExp(__pattern); __executed = __re.exec(__strOriginal); __executed[' + index + '] === ' + __expected[index] + '. Actual: ' + __executed[index]);
	}
}
