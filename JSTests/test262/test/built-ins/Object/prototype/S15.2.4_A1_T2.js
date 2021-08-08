// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Object prototype object has not prototype
es5id: 15.2.4_A1_T2
description: >
    Since the Object prototype object has not prototype, deleted
    toString method can not be found in prototype chain
---*/

//CHECK#1
if (Object.prototype.toString() == false) {
  throw new Test262Error('#1: Object prototype object has not prototype');
}

delete Object.prototype.toString;

// CHECK#2
try {
  Object.prototype.toString();
  throw new Test262Error('#2: Object prototype object has not prototype');
} catch (e) {
  if ((e instanceof TypeError) !== true) {
    throw new Test262Error('#1.1: delete Object.prototype.toString; Object.prototype.toString() throw a TypeError. Actual: ' + (e));
  }
}
//
