// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Equivalent to the expression RegExp.prototype.exec(string) != null
es5id: 15.10.6.3_A1_T6
description: >
    RegExp is /(z)((a+)?(b+)?(c))* / and tested string is
    (function(){return "zaacbbbcac"})()
---*/

var __re = /(z)((a+)?(b+)?(c))*/;

//CHECK#0
if (__re.test((function(){return "zaacbbbcac"})()) !== (__re.exec((function(){return "zaacbbbcac"})()) !== null)) {
	throw new Test262Error('#0: __re = /(z)((a+)?(b+)?(c))*/; __re.test((function(){return "zaacbbbcac"})()) === (__re.exec((function(){return "zaacbbbcac"})()) !== null)');
}
