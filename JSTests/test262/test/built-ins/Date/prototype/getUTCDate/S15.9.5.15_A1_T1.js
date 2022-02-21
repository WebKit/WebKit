// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getUTCDate" has { DontEnum } attributes
esid: sec-date.prototype.getutcdate
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.getUTCDate;
if (x === 1) {
  Date.prototype.getUTCDate = 2;
} else {
  Date.prototype.getUTCDate = 1;
}

assert.notSameValue(
  Date.prototype.getUTCDate,
  x,
  'The value of Date.prototype.getUTCDate is expected to not equal the value of `x`'
);

// TODO: Convert to verifyProperty() format.
