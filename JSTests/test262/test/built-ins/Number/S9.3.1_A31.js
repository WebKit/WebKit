// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: "The MV of HexDigit ::: f or of HexDigit ::: F is 15"
es5id: 9.3.1_A31
description: >
    Compare Number('0xF'), Number('0XF'), Number('0xf') and
    Number('0Xf') with 15
---*/

// CHECK#1
if (Number("0xf") !== 15) {
  throw new Test262Error('#1: Number("0xf") === 15. Actual: ' + (Number("0xf")));
}

// CHECK#2
if (Number("0xF") !== 15) {
  throw new Test262Error('#2: Number("0xF") === 15. Actual: ' + (Number("0xF")));
}

// CHECK#3
if (+("0Xf") !== 15) {
  throw new Test262Error('#3: +("0Xf") === 15. Actual: ' + (+("0Xf")));
}

// CHECK#4
if (Number("0XF") !== 15) {
  throw new Test262Error('#4: Number("0XF") === 15. Actual: ' + (Number("0XF")));
}
