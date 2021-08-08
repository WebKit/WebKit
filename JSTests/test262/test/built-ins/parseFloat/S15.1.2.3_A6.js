// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    parseFloat may interpret only a leading portion of the string as
    a number value; it ignores any characters that cannot be interpreted as part
    of the notation of an decimal literal, and no indication is given that any such
    characters were ignored.
esid: sec-parsefloat-string
description: Complex test without eval
includes: [decimalToHexString.js]
---*/

//CHECK
var errorCount = 0;
var count = 0;
var indexP;
var indexO = 0;
for (var index = 0; index <= 65535; index++) {
  if ((index < 0x0030) || (index > 0x0039)) {
    var hex = decimalToHexString(index);
    if (parseFloat("0.1e1" + String.fromCharCode(index)) !== 1) {
      if (indexO === 0) {
        indexO = index;
      } else {
        if ((index - indexP) !== 1) {
          if ((indexP - indexO) !== 0) {
            var hexP = decimalToHexString(indexP);
            var hexO = decimalToHexString(indexO);
            throw new Test262Error('#' + hexO + '-' + hexP + ' ');
          }
          else {
            var hexP = decimalToHexString(indexP);
            throw new Test262Error('#' + hexP + ' ');
          }
          indexO = index;
        }
      }
      indexP = index;
      errorCount++;
    }
    count++;
  }
}

if (errorCount > 0) {
  if ((indexP - indexO) !== 0) {
    var hexP = decimalToHexString(indexP);
    var hexO = decimalToHexString(indexO);
    throw new Test262Error('#' + hexO + '-' + hexP + ' ');
  } else {
    var hexP = decimalToHexString(indexP);
    throw new Test262Error('#' + hexP + ' ');
  }
  throw new Test262Error('Total error: ' + errorCount + ' bad Unicode character in ' + count + ' ');
}
