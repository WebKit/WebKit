// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: "The MV of HexDigit ::: e or of HexDigit ::: E is 14"
es5id: 9.3.1_A30
description: >
    Compare Number('0xE'), Number('0XE'), Number('0xe') and
    Number('0Xe') with 14
---*/

// CHECK#1
if (Number("0xe") !== 14) {
  throw new Test262Error('#1: Number("0xe") === 14. Actual: ' + (Number("0xe")));
}

// CHECK#2
if (Number("0xE") !== 14) {
  throw new Test262Error('#2: Number("0xE") === 14. Actual: ' + (Number("0xE")));
}

// CHECK#3
if (Number("0Xe") !== 14) {
  throw new Test262Error('#3: Number("0Xe") === 14. Actual: ' + (Number("0Xe")));
}

// CHECK#4
if (+("0XE") !== 14) {
  throw new Test262Error('#4: +("0XE") === 14. Actual: ' + (+("0XE")));
}
