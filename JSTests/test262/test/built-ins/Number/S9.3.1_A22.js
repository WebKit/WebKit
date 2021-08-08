// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: "The MV of DecimalDigit ::: 6 or of HexDigit ::: 6 is 6"
es5id: 9.3.1_A22
description: Compare Number('0x6') and Number('0X6') with 6
---*/

// CHECK#1
if (Number("6") !== 6) {
  throw new Test262Error('#1: Number("6") === 6. Actual: ' + (Number("6")));
}

// CHECK#2
if (+("0x6") !== 6) {
  throw new Test262Error('#2: +("0x6") === 6. Actual: ' + (+("0x6")));
}

// CHECK#3
if (Number("0X6") !== 6) {
  throw new Test262Error('#3: Number("0X6") === 6. Actual: ' + (Number("0X6")));
}
