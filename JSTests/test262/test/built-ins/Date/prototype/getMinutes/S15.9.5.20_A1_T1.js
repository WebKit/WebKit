// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.getminutes
info: The Date.prototype property "getMinutes" has { DontEnum } attributes
es5id: 15.9.5.20_A1_T1
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.getMinutes;
if (x === 1) {
  Date.prototype.getMinutes = 2;
} else {
  Date.prototype.getMinutes = 1;
}

assert.notSameValue(
  Date.prototype.getMinutes,
  x,
  'The value of Date.prototype.getMinutes is expected to not equal the value of `x`'
);

// TODO: Convert to verifyProperty() format.
