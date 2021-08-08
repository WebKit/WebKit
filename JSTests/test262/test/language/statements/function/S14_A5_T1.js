// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Identifer within a FunctionDeclaration can be written in both letters
    and unicode
es5id: 14_A5_T1
description: >
    Declaring a function with "function
    __\u0066\u0075\u006e\u0063(){return "both"}"
---*/

//////////////////////////////////////////////////////////////////////////////
//CHECK#1
if (__func() !== "both") {
	throw new Test262Error('#1: __func() === "both". Actual:  __func() ==='+ __func()  );
}
//
//////////////////////////////////////////////////////////////////////////////

function __func(){return "ascii"};
function \u005f\u005f\u0066\u0075\u006e\u0063(){return "unicode"};//__func in unicode
function __\u0066\u0075\u006e\u0063(){return "both"};//__func in unicode
