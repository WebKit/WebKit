// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    If thisArg is not null(defined) the called function is passed
    ToObject(thisArg) as the this value
es5id: 15.3.4.4_A5_T4
description: thisArg is function variable that return this
flags: [noStrict]
---*/

var f = function() {
  this.touched = true;
  return this;
};

var retobj = f.call(obj);

//CHECK#1
if (typeof obj !== "undefined") {
  throw new Test262Error('#1: If thisArg is not null(defined) the called function is passed ToObject(thisArg) as the this value');
}

//CHECK#2
if (!(retobj["touched"])) {
  throw new Test262Error('#2: If thisArg is not null(defined) the called function is passed ToObject(thisArg) as the this value');
}

var obj;
