// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype property "toLocaleDateString" has { DontEnum }
    attributes
esid: sec-date.prototype.tolocaledatestring
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.toLocaleDateString;
if (x === 1) {
  Date.prototype.toLocaleDateString = 2;
} else {
  Date.prototype.toLocaleDateString = 1;
}

assert.notSameValue(
  Date.prototype.toLocaleDateString,
  x,
  'The value of Date.prototype.toLocaleDateString is expected to not equal the value of `x`'
);

// TODO: Convert to verifyProperty() format.
