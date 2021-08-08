// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The MV of StrDecimalLiteral::: - StrUnsignedDecimalLiteral is the negative
    of the MV of StrUnsignedDecimalLiteral. (the negative of this 0 is also 0)
es5id: 9.3.1_A5_T3
description: Compare Number('-' + 'any_number') with -Number('any_number')
---*/

function dynaString(s1, s2) {
  return String(s1) + String(s2);
}

// CHECK#1
if (Number(dynaString("-", "0")) !== -Number("0")) {
  throw new Test262Error('#1: Number("-"+"0") === -Number("0")');
} else {
  // CHECK#2
  if (1 / Number(dynaString("-", "0")) !== -1 / Number("0")) {
    throw new Test262Error('#2: 1/Number("-"+"0") === -1/Number("0")');
  }
}

// CHECK#3
if (Number(dynaString("-Infi", "nity")) !== -Number("Infinity")) {
  throw new Test262Error('#3: Number("-Infi"+"nity") === -Number("Infinity")');
}

// CHECK#4
if (Number(dynaString("-12345", "67890")) !== -Number("1234567890")) {
  throw new Test262Error('#4: Number("-12345"+"67890") === -Number("1234567890")');
}

// CHECK#5
if (Number(dynaString("-1234.", "5678")) !== -Number("1234.5678")) {
  throw new Test262Error('#5: Number("-1234."+"5678") === -Number("1234.5678")');
}

// CHECK#6
if (Number(dynaString("-1234.", "5678e90")) !== -Number("1234.5678e90")) {
  throw new Test262Error('#6: Number("-1234."+"5678e90") === -Number("1234.5678e90")');
}

// CHECK#7
if (Number(dynaString("-1234.", "5678E90")) !== -Number("1234.5678E90")) {
  throw new Test262Error('#6: Number("-1234."+"5678E90") === -Number("1234.5678E90")');
}

// CHECK#8
if (Number(dynaString("-1234.", "5678e-90")) !== -Number("1234.5678e-90")) {
  throw new Test262Error('#6: Number("-1234."+"5678e-90") === -Number("1234.5678e-90")');
}

// CHECK#9
if (Number(dynaString("-1234.", "5678E-90")) !== -Number("1234.5678E-90")) {
  throw new Test262Error('#6: Number("-1234."+"5678E-90") === -Number("1234.5678E-90")');
}

// CHECK#10
if (Number(dynaString("-Infi", "nity")) !== Number.NEGATIVE_INFINITY) {
  throw new Test262Error('#3: Number("-Infi"+"nity") === Number.NEGATIVE_INFINITY');
}
