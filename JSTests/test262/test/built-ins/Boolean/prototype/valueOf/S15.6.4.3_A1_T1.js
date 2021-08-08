// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Boolean.prototype.valueOf() returns this boolean value
esid: sec-boolean.prototype.valueof
description: no arguments
---*/

//CHECK#1
if (Boolean.prototype.valueOf() !== false) {
  throw new Test262Error('#1: Boolean.prototype.valueOf() === false');
}

//CHECK#2
if ((new Boolean()).valueOf() !== false) {
  throw new Test262Error('#2: (new Boolean()).valueOf() === false');
}

//CHECK#3
if ((new Boolean(0)).valueOf() !== false) {
  throw new Test262Error('#3: (new Boolean(0)).valueOf() === false');
}

//CHECK#4
if ((new Boolean(-1)).valueOf() !== true) {
  throw new Test262Error('#4: (new Boolean(-1)).valueOf() === true');
}

//CHECK#5
if ((new Boolean(1)).valueOf() !== true) {
  throw new Test262Error('#5: (new Boolean(1)).valueOf() === true');
}

//CHECK#6
if ((new Boolean(new Object())).valueOf() !== true) {
  throw new Test262Error('#6: (new Boolean(new Object())).valueOf() === true');
}
