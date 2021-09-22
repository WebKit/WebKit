// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setUTCFullYear" has { DontEnum } attributes
esid: sec-date.prototype.setutcfullyear
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.setUTCFullYear;
if (x === 1) {
  Date.prototype.setUTCFullYear = 2;
} else {
  Date.prototype.setUTCFullYear = 1;
}

assert.notSameValue(
  Date.prototype.setUTCFullYear,
  x,
  'The value of Date.prototype.setUTCFullYear is expected to not equal the value of `x`'
);

// TODO: Convert to verifyProperty() format.
