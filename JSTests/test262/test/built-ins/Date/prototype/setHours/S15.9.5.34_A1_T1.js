// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setHours" has { DontEnum } attributes
esid: sec-date.prototype.sethours
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.setHours;
if (x === 1) {
  Date.prototype.setHours = 2;
} else {
  Date.prototype.setHours = 1;
}

assert.notSameValue(
  Date.prototype.setHours,
  x,
  'The value of Date.prototype.setHours is expected to not equal the value of `x`'
);

// TODO: Convert to verifyProperty() format.
