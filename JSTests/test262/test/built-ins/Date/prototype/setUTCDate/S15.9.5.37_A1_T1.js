// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setUTCDate" has { DontEnum } attributes
esid: sec-date.prototype.setutcdate
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.setUTCDate;
if (x === 1) {
  Date.prototype.setUTCDate = 2;
} else {
  Date.prototype.setUTCDate = 1;
}

assert.notSameValue(
  Date.prototype.setUTCDate,
  x,
  'The value of Date.prototype.setUTCDate is expected to not equal the value of `x`'
);

// TODO: Convert to verifyProperty() format.
