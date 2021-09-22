// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getUTCFullYear" has { DontEnum } attributes
esid: sec-date.prototype.getutcfullyear
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.getUTCFullYear;
if (x === 1) {
  Date.prototype.getUTCFullYear = 2;
} else {
  Date.prototype.getUTCFullYear = 1;
}

assert.notSameValue(
  Date.prototype.getUTCFullYear,
  x,
  'The value of Date.prototype.getUTCFullYear is expected to not equal the value of `x`'
);

// TODO: Convert to verifyProperty() format.
