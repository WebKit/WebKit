// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    Since when call is used for Function constructor themself new function instance creates
    and then first argument(thisArg) should be ignored
es5id: 15.3_A3_T4
description: First argument is this, and this have needed variable
---*/

var f = Function.call(this, "return planet;");

//CHECK#1
if (f() !== undefined) {
  throw new Test262Error('#1: ');
}

var planet = "mars";

//CHECK#2
if (f() !== "mars") {
  throw new Test262Error('#2: ');
}
