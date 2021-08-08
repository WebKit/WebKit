// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: String.prototype.slice (start, end)
es5id: 15.5.4.13_A1_T15
description: >
    Call slice without arguments. Instance is Number with
    prototype.slice = String.prototype.slice
---*/

var __num = 11.001002;

Number.prototype.slice = String.prototype.slice;


//////////////////////////////////////////////////////////////////////////////
//CHECK#1
if (__num.slice() !== "11.001002") {
  throw new Test262Error('#1: var __num = 11.001002; Number.prototype.slice = String.prototype.slice; __num.slice()==="11.001002". Actual: ' + __num.slice());
}
//
//////////////////////////////////////////////////////////////////////////////
