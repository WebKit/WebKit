// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: match returns array as specified in 15.10.6.2
es5id: 15.5.4.10_A2_T4
description: Regular expression is /\d{2}/g
---*/

var __matches = ["12", "34", "56", "78", "90"];

var __string = "123456abcde7890";

//////////////////////////////////////////////////////////////////////////////
//CHECK#1
if (__string.match(/\d{2}/g).length !== 5) {
  throw new Test262Error('#1: __string = "123456abcde7890"; __string.match(/\\d{2}/g).length=== 5. Actual: ' + __string.match(/\d{2}/g).length);
}
//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//CHECK#2
for (var mi = 0; mi < __matches.length; mi++) {
  if (__string.match(/\d{2}/g)[mi] !== __matches[mi]) {
    throw new Test262Error('#2.' + mi + ': __matches=["12", "34", "56", "78", "90"]; __string = "123456abcde7890"; __string.match(/\\d{2}/g)[' + mi + ']===__matches[' + mi + ']. Actual: ' + __string.match(/\d{2}/g)[mi]);
  }
}
//
//////////////////////////////////////////////////////////////////////////////
