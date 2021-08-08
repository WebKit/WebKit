// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Identifier in a FunctionExpression can be referenced from inside the
    FunctionExpression's FunctionBody to allow the function calling itself
    recursively
es5id: 13_A3_T2
description: >
    Creating a recursive function that calculates factorial, as a
    variable.  Function calls itself by the name of the variable
---*/

var __func = function (arg){
    if (arg === 1) {
    	return arg;
    } else {
    	return __func(arg-1)*arg;
    }
};

var fact_of_3 =  __func(3);

//////////////////////////////////////////////////////////////////////////////
//CHECK#1
if (fact_of_3 !== 6) {
	throw new Test262Error("#1: fact_of_3 === 6. Actual: fact_of_3 ==="+fact_of_3);
}
//
//////////////////////////////////////////////////////////////////////////////
