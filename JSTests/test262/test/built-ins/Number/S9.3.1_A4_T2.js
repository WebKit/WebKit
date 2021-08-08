// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The MV of StrDecimalLiteral::: + StrUnsignedDecimalLiteral is the MV of
    StrUnsignedDecimalLiteral
es5id: 9.3.1_A4_T2
description: Compare Number('+' + 'any_number') with Number('any_number')
---*/

function dynaString(s1, s2) {
  return String(s1) + String(s2);
}

// CHECK#1
if (Number(dynaString("+", "0")) !== Number("0")) {
  throw new Test262Error('#1: Number("+"+"0") === Number("0")');
} else {
  // CHECK#2
  if (1 / Number(dynaString("+", "0")) !== 1 / Number("0")) {
    throw new Test262Error('#2: 1/Number("+"+"0") === 1/Number("0")');
  }
}

// CHECK#3
if (Number(dynaString("+Infi", "nity")) !== Number("Infinity")) {
  throw new Test262Error('#3: Number("+Infin"+"ity") === Number("Infinity")');
}

// CHECK#4
if (Number(dynaString("+1234.", "5678")) !== Number("1234.5678")) {
  throw new Test262Error('#4: Number("+1234."+"5678") === Number("1234.5678")');
}

// CHECK#5
if (Number(dynaString("+1234.", "5678e90")) !== Number("1234.5678e90")) {
  throw new Test262Error('#5: Number("+1234."+"5678e90") === Number("1234.5678e90")');
}

// CHECK#6
if (Number(dynaString("+1234.", "5678E90")) !== Number("1234.5678E90")) {
  throw new Test262Error('#6: Number("+1234."+"5678E90") === Number("1234.5678E90")');
}

// CHECK#7
if (Number(dynaString("+1234.", "5678e-90")) !== Number("1234.5678e-90")) {
  throw new Test262Error('#7: Number("+1234."+"5678e-90") === Number("1234.5678e-90")');
}

// CHECK#8
if (Number(dynaString("+1234.", "5678E-90")) !== Number("1234.5678E-90")) {
  throw new Test262Error('#8: Number("+1234."+"5678E-90") === Number("1234.5678E-90")');
}
