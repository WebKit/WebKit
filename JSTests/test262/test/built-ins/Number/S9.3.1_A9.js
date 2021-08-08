// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The MV of StrUnsignedDecimalLiteral::: DecimalDigits. DecimalDigits ExponentPart
    is (the MV of the first DecimalDigits plus (the MV of the second DecimalDigits times
    10<sup><small>-n</small></sup>)) times 10<sup><small>e</small></sup>, where n is the number
    of characters in the second DecimalDigits and e is the MV of ExponentPart
es5id: 9.3.1_A9
description: >
    Compare Number('1234.5678e9') with
    (Number('1234')+(Number('5678')*1e-4))*1e9,  and +('1234.5678e-9')
    with (Number('1234')+(Number('5678')*1e-4))*1e-9
---*/

// CHECK#1
if (Number("1234.5678e9") !== (Number("1234") + (Number("5678") * 1e-4)) * 1e9) {
  throw new Test262Error('#1: Number("1234.5678e9") === (Number("1234")+(Number("5678")*1e-4))*1e9');
}

// CHECK#2
if (+("1234.5678e-9") !== (Number("1234") + (Number("5678") * 1e-4)) * 1e-9) {
  throw new Test262Error('#2: +("1234.5678e-9") === (Number("1234")+(Number("5678")*1e-4))*1e-9');
}
