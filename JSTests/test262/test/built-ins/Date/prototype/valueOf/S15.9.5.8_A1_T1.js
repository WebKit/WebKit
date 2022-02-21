// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "valueOf" has { DontEnum } attributes
esid: sec-date.prototype.valueof
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.valueOf;
if (x === 1) {
  Date.prototype.valueOf = 2;
} else {
  Date.prototype.valueOf = 1;
}

assert.notSameValue(
  Date.prototype.valueOf,
  x,
  'The value of Date.prototype.valueOf is expected to not equal the value of `x`'
);

// TODO: Convert to verifyProperty() format.
