// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    Once the exact MV for a string numeric literal has been
    determined, it is then rounded to a value of the Number type with 20
    significant digits by replacing each significant digit after the 20th
    with a 0 digit or the number value
es5id: 9.3.1_A32
description: Use various long numbers, for example, 1234567890.1234567890
---*/

// CHECK#1
if (Number("1234567890.1234567890") !== 1234567890.1234567890) {
  throw new Test262Error('#1: Number("1234567890.1234567890") === 1234567890.1234567890. Actual: ' + (Number("1234567890.1234567890")));
}

// CHECK#2
if (Number("1234567890.1234567890") !== 1234567890.1234567000) {
  throw new Test262Error('#2: Number("1234567890.1234567890") === 1234567890.1234567000. Actual: ' + (Number("1234567890.1234567890")));
}

// CHECK#3
if (+("1234567890.1234567890") === 1234567890.123456) {
  throw new Test262Error('#3: +("1234567890.1234567890") !== 1234567890.123456');
}

// CHECK#4
if (Number("0.12345678901234567890") !== 0.123456789012345678) {
  throw new Test262Error('#4: Number("0.12345678901234567890") === 0.123456789012345678. Actual: ' + (Number("0.12345678901234567890")));
}

// CHECK#4
if (Number("00.12345678901234567890") !== 0.123456789012345678) {
  throw new Test262Error('#4: Number("00.12345678901234567890") === 0.123456789012345678. Actual: ' + (Number("00.12345678901234567890")));
}
