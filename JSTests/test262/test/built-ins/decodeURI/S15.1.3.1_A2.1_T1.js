// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: If string.charAt(k) not equal "%", return this char
esid: sec-decodeuri-encodeduri
description: Complex tests
includes: [decimalToHexString.js]
---*/

for (var indexI = 0; indexI <= 65535; indexI++) {
  if (indexI !== 0x25) {
    var hex = decimalToHexString(indexI);
    try {
      var str = String.fromCharCode(indexI);
      if (decodeURI(str) !== str) {
        throw new Test262Error('#' + hex + ' ');
      }
    } catch (e) {
      throw new Test262Error('#' + hex + ' ');
    }
  }
}
