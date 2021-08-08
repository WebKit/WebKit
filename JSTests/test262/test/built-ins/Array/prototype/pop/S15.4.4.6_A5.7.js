// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The pop property of Array can't be used as constructor
esid: sec-array.prototype.pop
description: >
    If property does not implement the internal [[Construct]] method,
    throw a TypeError exception
---*/

//CHECK#1

try {
  new Array.prototype.pop();
  throw new Test262Error('#1.1: new Array.prototype.pop() throw TypeError. Actual: ' + (new Array.prototype.pop()));
} catch (e) {
  if ((e instanceof TypeError) !== true) {
    throw new Test262Error('#1.2: new Array.prototype.pop() throw TypeError. Actual: ' + (e));
  }
}
