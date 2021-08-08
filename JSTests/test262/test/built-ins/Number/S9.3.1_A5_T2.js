// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The MV of StrDecimalLiteral::: - StrUnsignedDecimalLiteral is the negative
    of the MV of StrUnsignedDecimalLiteral. (the negative of this 0 is also 0)
es5id: 9.3.1_A5_T2
description: Compare Number('-[or +]any_number') with -[or without -]any_number)
---*/

// CHECK#1
if (Number("1") !== 1) {
  throw new Test262Error('#1: Number("1") === 1');
}

// CHECK#2
if (Number("+1") !== 1) {
  throw new Test262Error('#3: Number("+1") === 1');
}

// CHECK#3
if (Number("-1") !== -1) {
  throw new Test262Error('#3: Number("-1") === -1');
}

// CHECK#4
if (Number("2") !== 2) {
  throw new Test262Error('#4: Number("2") === 2');
}

// CHECK#5
if (Number("+2") !== 2) {
  throw new Test262Error('#5: Number("+2") === 2');
}

// CHECK#6
if (Number("-2") !== -2) {
  throw new Test262Error('#6: Number("-2") === -2');
}

// CHECK#7
if (Number("3") !== 3) {
  throw new Test262Error('#7: Number("3") === 3');
}

// CHECK#8
if (Number("+3") !== 3) {
  throw new Test262Error('#8: Number("+3") === 3');
}

// CHECK#9
if (Number("-3") !== -3) {
  throw new Test262Error('#9: Number("-3") === -3');
}

// CHECK#10
if (Number("4") !== 4) {
  throw new Test262Error('#10: Number("4") === 4');
}

// CHECK#11
if (Number("+4") !== 4) {
  throw new Test262Error('#11: Number("+4") === 4');
}

// CHECK#12
if (Number("-4") !== -4) {
  throw new Test262Error('#12: Number("-4") === -4');
}

// CHECK#13
if (Number("5") !== 5) {
  throw new Test262Error('#13: Number("5") === 5');
}

// CHECK#14
if (Number("+5") !== 5) {
  throw new Test262Error('#14: Number("+5") === 5');
}

// CHECK#15
if (Number("-5") !== -5) {
  throw new Test262Error('#15: Number("-5") === -5');
}

// CHECK#16
if (Number("6") !== 6) {
  throw new Test262Error('#16: Number("6") === 6');
}

// CHECK#17
if (Number("+6") !== 6) {
  throw new Test262Error('#17: Number("+6") === 6');
}

// CHECK#18
if (Number("-6") !== -6) {
  throw new Test262Error('#18: Number("-6") === -6');
}

// CHECK#19
if (Number("7") !== 7) {
  throw new Test262Error('#19: Number("7") === 7');
}

// CHECK#20
if (Number("+7") !== 7) {
  throw new Test262Error('#20: Number("+7") === 7');
}

// CHECK#21
if (Number("-7") !== -7) {
  throw new Test262Error('#21: Number("-7") === -7');
}

// CHECK#22
if (Number("8") !== 8) {
  throw new Test262Error('#22: Number("8") === 8');
}

// CHECK#23
if (Number("+8") !== 8) {
  throw new Test262Error('#23: Number("+8") === 8');
}

// CHECK#24
if (Number("-8") !== -8) {
  throw new Test262Error('#24: Number("-8") === -8');
}

// CHECK#25
if (Number("9") !== 9) {
  throw new Test262Error('#25: Number("9") === 9');
}

// CHECK#26
if (Number("+9") !== 9) {
  throw new Test262Error('#26: Number("+9") === 9');
}

// CHECK#27
if (Number("-9") !== -9) {
  throw new Test262Error('#27: Number("-9") === -9');
}
