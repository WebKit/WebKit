// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    Result of number conversion from number value equals to the input
    argument (no conversion)
es5id: 9.3_A4.2_T1
description: >
    Number.NaN, +0, -0, Number.POSITIVE_INFINITY,
    Number.NEGATIVE_INFINITY,  Number.MAX_VALUE and Number.MIN_VALUE
    convert to Number by explicit transformation
---*/

// CHECK#1
assert.sameValue(Number(NaN), NaN, "NaN");

// CHECK#2
if (Number(+0) !== +0) {
  throw new Test262Error('#2.1: Number(+0) === 0. Actual: ' + (Number(+0)));
} else {
  if (1 / Number(+0) !== Number.POSITIVE_INFINITY) {
    throw new Test262Error('#2.2: Number(+0) === +0. Actual: -0');
  }
}

// CHECK#3
if (Number(-0) !== -0) {
  throw new Test262Error('#3.1: Number(-0) === 0. Actual: ' + (Number(-0)));
} else {
  if (1 / Number(-0) !== Number.NEGATIVE_INFINITY) {
    throw new Test262Error('#3.2: Number(-0) === -0. Actual: +0');
  }
}

// CHECK#4
if (Number(Number.POSITIVE_INFINITY) !== Number.POSITIVE_INFINITY) {
  throw new Test262Error('#4: Number(+Infinity) === +Infinity. Actual: ' + (Number(+Infinity)));
}

// CHECK#5
if (Number(Number.NEGATIVE_INFINITY) !== Number.NEGATIVE_INFINITY) {
  throw new Test262Error('#5: Number(-Infinity) === -Infinity. Actual: ' + (Number(-Infinity)));
}

// CHECK#6
if (Number(Number.MAX_VALUE) !== Number.MAX_VALUE) {
  throw new Test262Error('#6: Number(Number.MAX_VALUE) === Number.MAX_VALUE. Actual: ' + (Number(Number.MAX_VALUE)));
}

// CHECK#7
if (Number(Number.MIN_VALUE) !== Number.MIN_VALUE) {
  throw new Test262Error('#7: Number(Number.MIN_VALUE) === Number.MIN_VALUE. Actual: ' + (Number(Number.MIN_VALUE)));
}
