// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    RegExp.prototype.exec behavior depends on global property.
    Let global is true and let I = If ToInteger(lastIndex).
    Then if I<0 orI>length then set lastIndex to 0 and return null
es5id: 15.10.6.2_A5_T1
description: >
    First call /(?:ab|cd)\d?/g.exec("aac1dz2233a1bz12nm444ab42"), and
    then First call /(?:ab|cd)\d?/g.exec("aacd22")
---*/

var __re = /(?:ab|cd)\d?/g;
var __executed = __re.exec("aac1dz2233a1bz12nm444ab42");

var __expected = ["ab4"];
__expected.index = 21;
__expected.input = "aac1dz2233a1bz12nm444ab42";

//CHECK#1
if (__executed.length !== __expected.length) {
	throw new Test262Error('#1: __re = /(?:ab|cd)\\d?/g; __executed = __re.exec("aac1dz2233a1bz12nm444ab42"); __executed.length === ' + __expected.length + '. Actual: ' + __executed.length);
}

//CHECK#2
if (__executed.index !== __expected.index) {
	throw new Test262Error('#2: __re = /(?:ab|cd)\\d?/g; __executed = __re.exec("aac1dz2233a1bz12nm444ab42"); __executed.index === ' + __expected.index + '. Actual: ' + __executed.index);
}

//CHECK#3
if (__executed.input !== __expected.input) {
	throw new Test262Error('#3: __re = /(?:ab|cd)\\d?/g; __executed = __re.exec("aac1dz2233a1bz12nm444ab42"); __executed.input === ' + __expected.input + '. Actual: ' + __executed.input);
}

//CHECK#4
for(var index=0; index<__expected.length; index++) {
	if (__executed[index] !== __expected[index]) {
		throw new Test262Error('#4: __re = /(?:ab|cd)\\d?/g; __executed = __re.exec("aac1dz2233a1bz12nm444ab42"); __executed[' + index + '] === ' + __expected[index] + '. Actual: ' + __executed[index]);
	}
}

__executed = __re.exec("aacd22");

//CHECK#5
if (__executed) {
	throw new Test262Error('#5: __re = /(?:ab|cd)\\d?/g; __executed = __re.exec("aacd22"); __executed === true');
}

//CHECK#6
if (__re.lastIndex !== 0) {
	throw new Test262Error('#6: __re = /(?:ab|cd)\\d?/g; __executed = __re.exec("aacd22"); __re.lastIndex === 0. Actual: ' + (__re.lastIndex));
}
