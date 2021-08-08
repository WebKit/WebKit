// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    Since when call is used for Function constructor themself new function instance creates
    and then first argument(thisArg) should be ignored
es5id: 15.3_A3_T1
description: First argument is object
---*/

//CHECK#1 - does not throw
var f = Function.call(mars, "return name;");
var mars = {
  name: "mars",
  color: "red",
  number: 4
};

var f = Function.call(mars, "this.godname=\"ares\"; return this.color;");

var about_mars = f();

//CHECK#2
if (about_mars !== undefined) {
  throw new Test262Error('#2: When applied to the Function object itself, thisArg should be ignored');
}

//CHECK#3
if (this.godname !== "ares" && mars.godname === undefined) {
  throw new Test262Error('#3: When applied to the Function object itself, thisArg should be ignored');
}
