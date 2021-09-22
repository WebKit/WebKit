// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setFullYear" has { DontEnum } attributes
esid: sec-date.prototype.setfullyear
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.setFullYear;
if (x === 1) {
  Date.prototype.setFullYear = 2;
} else {
  Date.prototype.setFullYear = 1;
}

assert.notSameValue(
  Date.prototype.setFullYear,
  x,
  'The value of Date.prototype.setFullYear is expected to not equal the value of `x`'
);

// TODO: Convert to verifyProperty() format.
