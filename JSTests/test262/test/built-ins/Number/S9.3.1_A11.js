// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The MV of StrUnsignedDecimalLiteral:::. DecimalDigits ExponentPart
    is the MV of DecimalDigits times 10<sup><small>e-n</small></sup>, where n is
    the number of characters in DecimalDigits and e is the MV of ExponentPart
es5id: 9.3.1_A11
description: >
    Compare Number('.12345e6') with +('12345')*1e1,  and
    Number('.12345e-3') !== Number('12345')*1e-8
---*/

// CHECK#1
if (Number(".12345e6") !== +("12345") * 1e1) {
  throw new Test262Error('#1: Number(".12345e6") === +("12345")*1e1');
}

// CHECK#2
if (Number(".12345e-3") !== Number("12345") * 1e-8) {
  throw new Test262Error('#2: Number(".12345e-3") === Number("12345")*1e-8');
}
