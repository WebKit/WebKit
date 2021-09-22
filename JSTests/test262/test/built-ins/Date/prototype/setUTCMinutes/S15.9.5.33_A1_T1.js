// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setUTCMinutes" has { DontEnum } attributes
esid: sec-date.prototype.setutcminutes
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.setUTCMinutes;
if (x === 1) {
  Date.prototype.setUTCMinutes = 2;
} else {
  Date.prototype.setUTCMinutes = 1;
}

assert.notSameValue(
  Date.prototype.setUTCMinutes,
  x,
  'The value of Date.prototype.setUTCMinutes is expected to not equal the value of `x`'
);

// TODO: Convert to verifyProperty() format.
