// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    If thisArg is not null(defined) the called function is passed
    ToObject(thisArg) as the this value
es5id: 15.3.4.4_A5_T2
description: thisArg is boolean true
---*/

var obj = true;

var retobj = new Function("this.touched= true; return this;").call(obj);

//CHECK#1
if (typeof obj.touched !== "undefined") {
  throw new Test262Error('#1: If thisArg is not null(defined) the called function is passed ToObject(thisArg) as the this value');
}

//CHECK#2
if (!(retobj["touched"])) {
  throw new Test262Error('#2: If thisArg is not null(defined) the called function is passed ToObject(thisArg) as the this value');
}
