// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    If thisArg is null or undefined, the called function is passed the global
    object as the this value
es5id: 15.3.4.4_A3_T6
description: >
    Argument at call function is null and it called inside function
    declaration
flags: [noStrict]
---*/

function FACTORY() {
  (function() {
    this.feat = "kamon beyba"
  }).call(null);
};

var obj = new FACTORY;

//CHECK#1
if (this["feat"] !== "kamon beyba") {
  throw new Test262Error('#1: If thisArg is null or undefined, the called function is passed the global object as the this value');
}

//CHECK#2
if (typeof obj.feat !== "undefined") {
  throw new Test262Error('#1: If thisArg is null or undefined, the called function is passed the global object as the this value');
}
