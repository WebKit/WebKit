// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.gethours
info: The Date.prototype property "getHours" has { DontEnum } attributes
es5id: 15.9.5.18_A1_T1
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.getHours;
if (x === 1) {
  Date.prototype.getHours = 2;
} else {
  Date.prototype.getHours = 1;
}

assert.notSameValue(
  Date.prototype.getHours,
  x,
  'The value of Date.prototype.getHours is expected to not equal the value of `x`'
);

// TODO: Convert to verifyProperty() format.
