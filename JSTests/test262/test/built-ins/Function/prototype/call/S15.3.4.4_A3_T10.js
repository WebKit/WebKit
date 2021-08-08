// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    If thisArg is null or undefined, the called function is passed the global
    object as the this value
es5id: 15.3.4.4_A3_T10
description: Checking by using eval, no any arguments at call function
flags: [noStrict]
---*/

eval(" (function(){this.feat=1}).call()");

//CHECK#1
if (this["feat"] !== 1) {
  throw new Test262Error('#1: If thisArg is null or undefined, the called function is passed the global object as the this value');
}
