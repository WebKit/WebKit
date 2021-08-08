// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The MV of DecimalDigits ::: DecimalDigits DecimalDigit is
    (the MV of DecimalDigits times 10) plus the MV of DecimalDigit
es5id: 9.3.1_A13
description: Compare '12' with Number("1")*10+Number("2") and analogous
---*/

// CHECK#1
if (+("12") !== Number("1") * 10 + Number("2")) {
  throw new Test262Error('#1: +("12") === Number("1")*10+Number("2")');
}

// CHECK#2
if (Number("123") !== Number("12") * 10 + Number("3")) {
  throw new Test262Error('#2: Number("123") === Number("12")*10+Number("3")');
}

// CHECK#2
if (Number("1234") !== Number("123") * 10 + Number("4")) {
  throw new Test262Error('#2: Number("1234") === Number("123")*10+Number("4")');
}
