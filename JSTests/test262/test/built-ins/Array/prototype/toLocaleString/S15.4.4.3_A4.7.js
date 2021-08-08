// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.tolocalestring
info: The toLocaleString property of Array can't be used as constructor
es5id: 15.4.4.3_A4.7
description: >
    If property does not implement the internal [[Construct]] method,
    throw a TypeError exception
---*/

//CHECK#1

try {
  new Array.prototype.toLocaleString();
  throw new Test262Error('#1.1: new Array.prototype.toLocaleString() throw TypeError. Actual: ' + (new Array.prototype.toLocaleString()));
} catch (e) {
  if ((e instanceof TypeError) !== true) {
    throw new Test262Error('#1.2: new Array.prototype.toLocaleString() throw TypeError. Actual: ' + (e));
  }
}
