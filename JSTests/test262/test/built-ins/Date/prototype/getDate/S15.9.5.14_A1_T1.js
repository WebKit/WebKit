// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getDate" has { DontEnum } attributes
esid: sec-date.prototype.getdate
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.getDate;
if (x === 1) {
  Date.prototype.getDate = 2;
} else {
  Date.prototype.getDate = 1;
}

assert.notSameValue(
  Date.prototype.getDate,
  x,
  'The value of Date.prototype.getDate is expected to not equal the value of `x`'
);

// TODO: Convert to verifyProperty() format.
