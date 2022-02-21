// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getUTCHours" has { DontEnum } attributes
esid: sec-date.prototype.getutchours
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.getUTCHours;
if (x === 1) {
  Date.prototype.getUTCHours = 2;
} else {
  Date.prototype.getUTCHours = 1;
}

assert.notSameValue(
  Date.prototype.getUTCHours,
  x,
  'The value of Date.prototype.getUTCHours is expected to not equal the value of `x`'
);

// TODO: Convert to verifyProperty() format.
