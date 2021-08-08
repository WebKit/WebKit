// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The shift property of Array can't be used as constructor
esid: sec-array.prototype.shift
description: >
    If property does not implement the internal [[Construct]] method,
    throw a TypeError exception
---*/

//CHECK#1

try {
  new Array.prototype.shift();
  throw new Test262Error('#1.1: new Array.prototype.shift() throw TypeError. Actual: ' + (new Array.prototype.shift()));
} catch (e) {
  if ((e instanceof TypeError) !== true) {
    throw new Test262Error('#1.2: new Array.prototype.shift() throw TypeError. Actual: ' + (e));
  }
}
