// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    If string.charAt(k) in [0x0000 - 0x007F]\[uriUnescaped], return 1 octet
    (00000000 0zzzzzzz -> 0zzzzzzz)
esid: sec-encodeuricomponent-uricomponent
description: Complex tests, use RFC 3629
includes: [decimalToHexString.js]
---*/

var uriUnescaped = ["-", "_", ".", "!", "~", "*", "'", "(", ")", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9"];
var errorCount = 0;
var count = 0;
var indexP;
var indexO = 0;

l:
  for (var index = 0x0000; index <= 0x007F; index++) {
    count++;
    var str = String.fromCharCode(index);
    for (var indexC = 0; indexC < uriUnescaped.length; indexC++) {
      if (uriUnescaped[indexC] === str) continue l;
    }
    if (encodeURIComponent(str).toUpperCase() === decimalToPercentHexString(index)) continue l;

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
