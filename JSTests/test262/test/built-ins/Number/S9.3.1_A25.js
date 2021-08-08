// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: "The MV of DecimalDigit ::: 9 or of HexDigit ::: 9 is 9"
es5id: 9.3.1_A25
description: Compare Number('0x9') and Number('0X9') with 9
---*/

// CHECK#1
if (Number("9") !== 9) {
  throw new Test262Error('#1: Number("9") === 9. Actual: ' + (Number("9")));
}

// CHECK#2
if (+("0x9") !== 9) {
  throw new Test262Error('#2: +("0x9") === 9. Actual: ' + (+("0x9")));
}

// CHECK#3
if (Number("0X9") !== 9) {
  throw new Test262Error('#3: Number("0X9") === 9. Actual: ' + (Number("0X9")));
}
