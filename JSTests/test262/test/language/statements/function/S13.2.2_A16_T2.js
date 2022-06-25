// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: FunctionExpression within a new statement is admitted
es5id: 13.2.2_A16_T2
description: >
    Using "var __obj = new function __func(arg){this.prop=arg;}(5)" as
    FunctionExpression
---*/

//////////////////////////////////////////////////////////////////////////////
//CHECK#1
if (typeof __func !== "undefined") {
	throw new Test262Error('#1: typeof __func === "undefined". Actual: typeof __func ==='+typeof __func);
}
//
//////////////////////////////////////////////////////////////////////////////

var __obj = new function __func(arg){this.prop=arg;}(5);

//////////////////////////////////////////////////////////////////////////////
//CHECK#2
if (__obj.prop !== 5) {
	throw new Test262Error('#2: __obj.prop === 5. Actual: __obj.prop ==='+__obj.prop);
}
//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//CHECK#3
if (typeof __func !== "undefined") {
	throw new Test262Error('#3: typeof __func === "undefined". Actual: typeof __func ==='+typeof __func);
}
//
//////////////////////////////////////////////////////////////////////////////
