// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The value of the internal [[Prototype]] property of the Boolean
    prototype object is the Object prototype object
esid: sec-properties-of-the-boolean-prototype-object
description: Checking Object.prototype.isPrototypeOf(Boolean.prototype)
---*/

//CHECK#1
if (!Object.prototype.isPrototypeOf(Boolean.prototype)) {
  throw new Test262Error('#1: Object prototype object is the prototype of Boolean prototype object');
}
