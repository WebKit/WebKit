// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: let P be ToString(pattern) and let F be ToString(flags)
es5id: 15.10.4.1_A8_T13
description: >
    Pattern is "1" and flags is {toString:function(){throw "intostr";}
    }
---*/

//CHECK#1
try {
	throw new Test262Error('#1.1: new RegExp("1", {toString:function(){throw "intostr";}}) throw "intostr". Actual: ' + (new RegExp("1", {toString:function(){throw "intostr";}})));
} catch (e) {
	if (e !== "intostr" ) {
		throw new Test262Error('#1.2: new RegExp("1", {toString:function(){throw "intostr";}}) throw "intostr". Actual: ' + (e));
	}
}
