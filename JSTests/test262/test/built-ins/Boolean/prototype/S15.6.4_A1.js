// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Boolean prototype object is itself a Boolean object
    (its [[Class]] is "Boolean") whose value is false
esid: sec-properties-of-the-boolean-prototype-object
description: Checking type and value of Boolean.prototype
---*/

//CHECK#1
if (typeof Boolean.prototype !== "object") {
  throw new Test262Error('#1: typeof Boolean.prototype === "object"');
}

//CHECK#2
if (Boolean.prototype != false) {
  throw new Test262Error('#2: Boolean.prototype == false');
}

delete Boolean.prototype.toString;

if (Boolean.prototype.toString() !== "[object Boolean]") {
  throw new Test262Error('#3: The [[Class]] property of the Boolean prototype object is set to "Boolean"');
}
