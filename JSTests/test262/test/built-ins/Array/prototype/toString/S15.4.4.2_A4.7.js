// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.tostring
info: The toString property of Array can't be used as constructor
es5id: 15.4.4.2_A4.7
description: >
    If property does not implement the internal [[Construct]] method,
    throw a TypeError exception
---*/

//CHECK#1

try {
  new Array.prototype.toString();
  throw new Test262Error('#1.1: new Array.prototype.toString() throw TypeError. Actual: ' + (new Array.prototype.toString()));
} catch (e) {
  if ((e instanceof TypeError) !== true) {
    throw new Test262Error('#1.2: new Array.prototype.toString() throw TypeError. Actual: ' + (e));
  }
}
