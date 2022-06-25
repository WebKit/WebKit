// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: replace with regexp /(uid=)(\d+)/ returns
es5id: 15.5.4.11_A3_T1
description: replaceValue is "$11" + 15
---*/

var __str = 'uid=31';
var __re = /(uid=)(\d+)/;

//////////////////////////////////////////////////////////////////////////////
//CHECK#1
if (__str.replace(__re, "$11" + 15) !== 'uid=115') {
  throw new Test262Error('#1: var __str = \'uid=31\'; var __re = /(uid=)(\d+)/; __str.replace(__re, "$11" + 15)===\'uid=115\'. Actual: ' + __str.replace(__re, "$11" + 15));
}
//
//////////////////////////////////////////////////////////////////////////////
