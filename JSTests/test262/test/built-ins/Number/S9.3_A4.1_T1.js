// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    Result of number conversion from number value equals to the input
    argument (no conversion)
es5id: 9.3_A4.1_T1
description: >
    Some numbers including Number.MAX_VALUE and Number.MIN_VALUE are
    converted to Number with explicit transformation
---*/

// CHECK#1
if (Number(13) !== 13) {
  throw new Test262Error('#1: Number(13) === 13. Actual: ' + (Number(13)));
}

// CHECK#2
if (Number(-13) !== -13) {
  throw new Test262Error('#2: Number(-13) === -13. Actual: ' + (Number(-13)));
}

// CHECK#3
if (Number(1.3) !== 1.3) {
  throw new Test262Error('#3: Number(1.3) === 1.3. Actual: ' + (Number(1.3)));
}

// CHECK#4
if (Number(-1.3) !== -1.3) {
  throw new Test262Error('#4: Number(-1.3) === -1.3. Actual: ' + (Number(-1.3)));
}
