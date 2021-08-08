// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The [[Prototype]] property of the newly constructed object
    is set to the original Number prototype object, the one that is the
    initial value of Number.prototype
es5id: 15.7.2.1_A2
description: Checking prototype property of the newly created objects
---*/

// CHECK#1
var x1 = new Number(1);
if (typeof x1.constructor.prototype !== "object") {
  throw new Test262Error('#1: typeof x1.constructor.prototype === "object"');
}

//CHECK#2
var x2 = new Number(2);
if (!Number.prototype.isPrototypeOf(x2)) {
  throw new Test262Error('#2: Number.prototype.isPrototypeOf(x2)');
}

//CHECK#3
var x3 = new Number(3);
if (Number.prototype !== x3.constructor.prototype) {
  throw new Test262Error('#3: Number.prototype === x3.constructor.prototype');
}
