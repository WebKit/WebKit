// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    Since when call is used for Function constructor themself new function instance creates
    and then first argument(thisArg) should be ignored
es5id: 15.3_A3_T5
description: >
    First argument is this, and this don`t have needed variable.
    Function return this.var_name
---*/

var f = Function.call(this, "return this.planet;");
var g = Function.call(this, "return this.color;");

//CHECK#1
if (f() !== undefined) {
  throw new Test262Error('#2: ');
}

var planet = "mars";

//CHECK#2
if (f() !== "mars") {
  throw new Test262Error('#2: ');
}

//CHECK#3
if (g() !== undefined) {
  throw new Test262Error('#3: ');
}

this.color = "red";

//CHECK#4
if (g() !== "red") {
  throw new Test262Error('#4: ');
}
