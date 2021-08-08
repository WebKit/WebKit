// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: let P be ToString(pattern) and let F be ToString(flags)
es5id: 15.10.4.1_A8_T9
description: Pattern is 1 and flags is new Object("gi")
---*/

var __re = new RegExp(1, new Object("gi"));

//CHECK#1
if (__re.ignoreCase !== true) {
	throw new Test262Error('#1: __re = new RegExp(1, new Object("gi")); __re.ignoreCase === true. Actual: ' + (__re.ignoreCase));
}

//CHECK#2
if (__re.multiline !== false) {
	throw new Test262Error('#2: __re = new RegExp(1, new Object("gi")); __re.multiline === false. Actual: ' + (__re.multiline));
}

//CHECK#3
if (__re.global !== true) {
	throw new Test262Error('#3: __re = new RegExp(1, new Object("gi")); __re.global === true. Actual: ' + (__re.global));
}

//CHECK#4
if (__re.lastIndex !== 0) {
	throw new Test262Error('#4: __re = new RegExp(1, new Object("gi")); __re.lastIndex === 0. Actual: ' + (__re.lastIndex));
}

//CHECK#5
if (typeof __re.source === "undefined") {
	throw new Test262Error('#5: __re = new RegExp(1, new Object("gi")); typeof __re.source !== "undefined"');
}
