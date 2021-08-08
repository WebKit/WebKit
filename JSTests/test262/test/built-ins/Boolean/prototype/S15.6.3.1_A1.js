// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The initial value of Boolean.prototype is the Boolean
    prototype object
esid: sec-boolean.prototype
description: Checking Boolean.prototype property
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
