// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: match returns array as specified in 15.10.6.2
es5id: 15.5.4.10_A2_T2
description: Regular expression is /34/g
---*/

var __matches = ["34", "34", "34"];

var __string = "343443444";

//////////////////////////////////////////////////////////////////////////////
//CHECK#1
if (__string.match(/34/g).length !== 3) {
  throw new Test262Error('#1: __string = "343443444"; __string.match(/34/g).length=== 3. Actual: ' + __string.match(/34/g).length);
}
//
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//CHECK#2
for (var mi = 0; mi < __matches.length; mi++) {
  if (__string.match(/34/g)[mi] !== __matches[mi]) {
    throw new Test262Error('#2.' + mi + ': __matches=["34","34","34"]; __string = "343443444"; __string.match(/34/g)[' + mi + ']===__matches[' + mi + ']. Actual: ' + __string.match(/34/g)[mi]);
  }
}
//
//////////////////////////////////////////////////////////////////////////////
