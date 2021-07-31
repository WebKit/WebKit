// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: toLocaleString function returns the result of calling toString()
es5id: 15.2.4.3_A1
description: >
    Checking the type of Object.prototype.toLocaleString and the
    returned result
---*/

//CHECK#1
if (typeof Object.prototype.toLocaleString !== "function") {
  throw new Test262Error('#1: toLocaleString method defined');
}

//CHECK#2
if (Object.prototype.toLocaleString() !== Object.prototype.toString()) {
  throw new Test262Error('#1: toLocaleString function returns the result of calling toString()');
}

//CHECK#2
if ({}.toLocaleString() !== {}.toString()) {
  throw new Test262Error('#2: toLocaleString function returns the result of calling toString()');
}
