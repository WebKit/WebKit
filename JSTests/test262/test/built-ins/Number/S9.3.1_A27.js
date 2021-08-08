// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: "The MV of HexDigit ::: b or of HexDigit ::: B is 11"
es5id: 9.3.1_A27
description: >
    Compare Number('0xB'), Number('0XB'), Number('0xb') and
    Number('0Xb') with 11
---*/

// CHECK#1
if (Number("0xb") !== 11) {
  throw new Test262Error('#1: Number("0xb") === 11. Actual: ' + (Number("0xb")));
}

// CHECK#2
if (Number("0xB") !== 11) {
  throw new Test262Error('#2: Number("0xB") === 11. Actual: ' + (Number("0xB")));
}

// CHECK#3
if (+("0Xb") !== 11) {
  throw new Test262Error('#3: +("0Xb") === 11. Actual: ' + (+("0Xb")));
}

// CHECK#4
if (Number("0XB") !== 11) {
  throw new Test262Error('#4: Number("0XB") === 11. Actual: ' + (Number("0XB")));
}
