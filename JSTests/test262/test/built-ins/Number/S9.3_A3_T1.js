// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    Result of number conversion from boolean value is 1 if the argument is
    true, else is +0
es5id: 9.3_A3_T1
description: False and true convert to Number by explicit transformation
---*/

// CHECK#1
if (Number(false) !== +0) {
  throw new Test262Error('#1.1: Number(false) === 0. Actual: ' + (Number(false)));
} else {
  if (1 / Number(false) !== Number.POSITIVE_INFINITY) {
    throw new Test262Error('#1.2: Number(false) === +0. Actual: -0');
  }
}

// CHECK#2
if (Number(true) !== 1) {
  throw new Test262Error('#2: Number(true) === 1. Actual: ' + (Number(true)));
}
