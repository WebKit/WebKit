// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.valueof
info: |
    Result of ToInteger(value) conversion is the result of computing
    sign(ToNumber(value)) * floor(abs(ToNumber(value)))
es5id: 9.4_A3_T1
description: For testing constructor Date(Number) is used
---*/

// CHECK#1
var d1 = new Date(6.54321);
if (d1.valueOf() !== 6) {
  throw new Test262Error('#1: var d1 = new Date(6.54321); d1.valueOf() === 6;');
}

// CHECK#2
var d2 = new Date(-6.54321);
if (d2.valueOf() !== -6) {
  throw new Test262Error('#2: var d2 = new Date(-6.54321); d2.valueOf() === -6;');
}

// CHECK#3
var d3 = new Date(6.54321e2);
if (d3.valueOf() !== 654) {
  throw new Test262Error('#3: var d3 = new Date(6.54321e2); d3.valueOf() === 654;');
}

// CHECK#4
var d4 = new Date(-6.54321e2);
if (d4.valueOf() !== -654) {
  throw new Test262Error('#4: var d4 = new Date(-6.54321e2); d4.valueOf() === -654;');
}

// CHECK#5
var d5 = new Date(0.654321e1);
if (d5.valueOf() !== 6) {
  throw new Test262Error('#5: var d5 = new Date(0.654321e1); d5.valueOf() === 6;');
}

// CHECK#6
var d6 = new Date(-0.654321e1);
if (d6.valueOf() !== -6) {
  throw new Test262Error('#6: var d6 = new Date(-0.654321e1); d6.valueOf() === -6;');
}

// CHECK#7
var d7 = new Date(true);
if (d7.valueOf() !== 1) {
  throw new Test262Error('#7: var d7 = new Date(true); d7.valueOf() === 1;');
}

// CHECK#8
var d8 = new Date(false);
if (d8.valueOf() !== 0) {
  throw new Test262Error('#8: var d8 = new Date(false); d8.valueOf() === 0;');
}

// CHECK#9
var d9 = new Date(1.23e15);
if (d9.valueOf() !== 1.23e15) {
  throw new Test262Error('#9: var d9 = new Date(1.23e15); d9.valueOf() === 1.23e15;');
}

// CHECK#10
var d10 = new Date(-1.23e15);
if (d10.valueOf() !== -1.23e15) {
  throw new Test262Error('#10: var d10 = new Date(-1.23e15); d10.valueOf() === -1.23e15;');
}

// CHECK#11
var d11 = new Date(1.23e-15);
if (d11.valueOf() !== 0) {
  throw new Test262Error('#11: var d11 = new Date(1.23e-15); d11.valueOf() === 0;');
}

// CHECK#12
var d12 = new Date(-1.23e-15);
if (d12.valueOf() !== -0) {
  throw new Test262Error('#12: var d12 = new Date(-1.23e-15); d12.valueOf() === -0;');
}
