// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Equivalent to the expression RegExp.prototype.exec(string) != null
es5id: 15.10.6.3_A1_T12
description: >
    RegExp is /\.14/ and tested string is {toString:function(){return
    Math.PI;}}
---*/

var __string = {toString:function(){return Math.PI;}};
var __re = /\.14/;

//CHECK#0
if (__re.test(__string) !== (__re.exec(__string) !== null)) {
	throw new Test262Error('#0: var __string = {toString:function(){return Math.PI;}}; __re = /\\.14/; __re.test(__string) === (__re.exec(__string) !== null)');
}
