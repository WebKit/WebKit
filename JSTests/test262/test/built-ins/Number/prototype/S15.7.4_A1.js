// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Number prototype object is itself a Number object
    (its [[Class]] is "Number") whose value is +0
es5id: 15.7.4_A1
description: Checking type and value of Number.prototype property
---*/

//CHECK#1
if (typeof Number.prototype !== "object") {
  throw new Test262Error('#1: typeof Number.prototype === "object"');
}

//CHECK#2
if (Number.prototype != 0) {
  throw new Test262Error('#2: Number.prototype == +0');
} else if (1 / Number.prototype != Number.POSITIVE_INFINITY) {
  throw new Test262Error('#2: Number.prototype == +0');
}

delete Number.prototype.toString;

if (Number.prototype.toString() !== "[object Number]") {
  throw new Test262Error('#3: The [[Class]] property of the Number prototype object is set to "Number"');
}
