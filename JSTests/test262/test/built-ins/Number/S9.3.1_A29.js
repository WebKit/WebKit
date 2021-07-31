// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: "The MV of HexDigit ::: d or of HexDigit ::: D is 13"
es5id: 9.3.1_A29
description: >
    Compare Number('0xD'), Number('0XD'), Number('0xd') and
    Number('0Xd') with 13
---*/

// CHECK#1
if (+("0xd") !== 13) {
  throw new Test262Error('#1: +("0xd") === 13. Actual: ' + (+("0xd")));
}

// CHECK#2
if (Number("0xD") !== 13) {
  throw new Test262Error('#2: Number("0xD") === 13. Actual: ' + (Number("0xD")));
}

// CHECK#3
if (Number("0Xd") !== 13) {
  throw new Test262Error('#3: Number("0Xd") === 13. Actual: ' + (Number("0Xd")));
}

// CHECK#4
if (Number("0XD") !== 13) {
  throw new Test262Error('#4: Number("0XD") === 13. Actual: ' + (Number("0XD")));
}
