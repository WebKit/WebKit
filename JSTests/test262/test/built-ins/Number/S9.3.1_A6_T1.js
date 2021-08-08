// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The MV of StrUnsignedDecimalLiteral::: Infinity is 10<sup><small>10000</small></sup>
    (a value so large that it will round to <b><tt>+&infin;</tt></b>)
es5id: 9.3.1_A6_T1
description: >
    Compare Number('Infinity') with Number.POSITIVE_INFINITY,
    10e10000, 10E10000 and Number("10e10000")
---*/

// CHECK#1
if (Number("Infinity") !== Number.POSITIVE_INFINITY) {
  throw new Test262Error('#1: Number("Infinity") === Number.POSITIVE_INFINITY');
}

// CHECK#2
if (Number("Infinity") !== 10e10000) {
  throw new Test262Error('#2: Number("Infinity") === 10e10000');
}

// CHECK#3
if (Number("Infinity") !== 10E10000) {
  throw new Test262Error('#3: Number("Infinity") === 10E10000');
}

// CHECK#4
if (Number("Infinity") !== Number("10e10000")) {
  throw new Test262Error('#4: Number("Infinity") === Number("10e10000")');
}
