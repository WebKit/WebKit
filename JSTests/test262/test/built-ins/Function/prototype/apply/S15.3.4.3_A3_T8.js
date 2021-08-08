// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    If thisArg is null or undefined, the called function is passed the global
    object as the this value
es5id: 15.3.4.3_A3_T8
description: >
    Argument at apply function is undefined and it called inside
    function declaration
flags: [noStrict]
---*/

(function FACTORY() {
  (function() {
    this.feat = "kamon beyba"
  }).apply(undefined);
})();

//CHECK#1
if (this["feat"] !== "kamon beyba") {
  throw new Test262Error('#1: If thisArg is null or undefined, the called function is passed the global object as the this value');
}
