// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: String.prototype.substring (start, end) returns a string value(not object)
es5id: 15.5.4.15_A2_T6
description: start is negative float number, end is 0
---*/

var __string = new String("this is a string object");

//////////////////////////////////////////////////////////////////////////////
//CHECK#1
if (__string.substring(-0.01, 0) !== "") {
  throw new Test262Error('#1: __string = new String("this is a string object"); __string.substring(-0.01,0) === "". Actual: ' + __string.substring(-0.01, 0));
}
//
//////////////////////////////////////////////////////////////////////////////
