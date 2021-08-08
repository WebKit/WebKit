// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: If y is +Infinity and x !== y, return false
es5id: 11.8.4_A4.6
description: x is number primitive
---*/

//CHECK#1
if ((0 >= Number.POSITIVE_INFINITY) !== false) {
  throw new Test262Error('#1: (0 >= +Infinity) === false');
}

//CHECK#2
if ((1.1 >= Number.POSITIVE_INFINITY) !== false) {
  throw new Test262Error('#2: (1.1 >= +Infinity) === false');
}

//CHECK#3
if ((-1.1 >= Number.POSITIVE_INFINITY) !== false) {
  throw new Test262Error('#3: (-1.1 >= +Infinity) === false');
}

//CHECK#4
if ((Number.NEGATIVE_INFINITY >= Number.POSITIVE_INFINITY) !== false) {
  throw new Test262Error('#4: (-Infinity >= +Infinity) === false');
}

//CHECK#5
if ((Number.MAX_VALUE >= Number.POSITIVE_INFINITY) !== false) {
  throw new Test262Error('#5: (Number.MAX_VALUE >= +Infinity) === false');
}

//CHECK#6
if ((Number.MIN_VALUE >= Number.POSITIVE_INFINITY) !== false) {
  throw new Test262Error('#6: (Number.MIN_VALUE >= +Infinity) === false');
}
