// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: If string.charAt(k) not equal "%", return this char
esid: sec-decodeuri-encodeduri
description: Complex tests
includes: [decimalToHexString.js]
---*/

//CHECK
var errorCount = 0;
var count = 0;
for (var indexI = 0; indexI <= 65535; indexI++) {
  if (indexI !== 0x25) {
    var hex = decimalToHexString(indexI);
    try {
      var str = String.fromCharCode(indexI);
      if (decodeURI(str) !== str) {
        throw new Test262Error('#' + hex + ' ');
        errorCount++;
      }
    } catch (e) {
      throw new Test262Error('#' + hex + ' ');
      errorCount++;
    }
    count++;
  }
}

if (errorCount > 0) {
  throw new Test262Error('Total error: ' + errorCount + ' bad Unicode character in ' + count);
}
