// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    RegExp.prototype.exec behavior depends on global property.
    If global is true next exec calling start to match from lastIndex position
es5id: 15.10.6.2_A4_T12
description: >
    Call first exec, then set re.lastIndex =
    {toString:function(){return 12;},valueOf:function(){return {};}}
    and again call exec
---*/

var __re = /(?:ab|cd)\d?/g;

var __executed = __re.exec("aacd2233ab12nm444ab42");

var __expected = ["cd2"];
__expected.index = 2;
__expected.input = "aacd2233ab12nm444ab42";

//CHECK#1
if (__executed.length !== __expected.length) {
	throw new Test262Error('#1: __re = /(?:ab|cd)\\d?/g; __executed = __re.exec("aacd2233ab12nm444ab42"); __executed.length === ' + __expected.length + '. Actual: ' + __executed.length);
}

//CHECK#2
if (__executed.index !== __expected.index) {
	throw new Test262Error('#2: __re = /(?:ab|cd)\\d?/g; __executed = __re.exec("aacd2233ab12nm444ab42"); __executed.index === ' + __expected.index + '. Actual: ' + __executed.index);
}

//CHECK#3
if (__executed.input !== __expected.input) {
	throw new Test262Error('#3: __re = /(?:ab|cd)\\d?/g; __executed = __re.exec("aacd2233ab12nm444ab42"); __executed.input === ' + __expected.input + '. Actual: ' + __executed.input);
}

//CHECK#4
for(var index=0; index<__expected.length; index++) {
	if (__executed[index] !== __expected[index]) {
		throw new Test262Error('#4: __re = /(?:ab|cd)\\d?/g; __executed = __re.exec("aacd2233ab12nm444ab42"); __executed[' + index + '] === ' + __expected[index] + '. Actual: ' + __executed[index]);
	}
}

var __obj = {toString:function(){return 12;},valueOf:function(){return {};}};

__re.lastIndex = __obj;

__executed = __re.exec("aacd2233ab12nm444ab42");

__expected = ["ab4"];
__expected.index = 17;
__expected.input = "aacd2233ab12nm444ab42";

//CHECK#5
if (__executed.length !== __expected.length) {
	throw new Test262Error('#5: __re = /(?:ab|cd)\\d?/g; __obj = {toString:function(){return 12;},valueOf:function(){return {};}}; __re.lastIndex = __obj; __executed = __re.exec("aacd2233ab12nm444ab42"); __executed.length === ' + __expected.length + '. Actual: ' + __executed.length);
}

//CHECK#6
if (__executed.index !== __expected.index) {
	throw new Test262Error('#6: __re = /(?:ab|cd)\\d?/g; __obj = {toString:function(){return 12;},valueOf:function(){return {};}}; __re.lastIndex = __obj; __executed = __re.exec("aacd2233ab12nm444ab42"); __executed.index === ' + __expected.index + '. Actual: ' + __executed.index);
}

//CHECK#7
if (__executed.input !== __expected.input) {
	throw new Test262Error('#7: __re = /(?:ab|cd)\\d?/g; __obj = {toString:function(){return 12;},valueOf:function(){return {};}}; __re.lastIndex = __obj; __executed = __re.exec("aacd2233ab12nm444ab42"); __executed.input === ' + __expected.input + '. Actual: ' + __executed.input);
}

//CHECK#8
for(var index=0; index<__expected.length; index++) {
	if (__executed[index] !== __expected[index]) {
		throw new Test262Error('#8: __re = /(?:ab|cd)\\d?/g; __obj = {toString:function(){return 12;},valueOf:function(){return {};}}; __re.lastIndex = __obj; __executed = __re.exec("aacd2233ab12nm444ab42"); __executed[' + index + '] === ' + __expected[index] + '. Actual: ' + __executed[index]);
	}
}
