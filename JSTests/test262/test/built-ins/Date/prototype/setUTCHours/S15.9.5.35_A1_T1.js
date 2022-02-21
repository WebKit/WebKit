// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setUTCHours" has { DontEnum } attributes
esid: sec-date.prototype.setutchours
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.setUTCHours;
if (x === 1) {
  Date.prototype.setUTCHours = 2;
} else {
  Date.prototype.setUTCHours = 1;
}

assert.notSameValue(
  Date.prototype.setUTCHours,
  x,
  'The value of Date.prototype.setUTCHours is expected to not equal the value of `x`'
);

// TODO: Convert to verifyProperty() format.
