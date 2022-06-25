// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getUTCDay" has { DontEnum } attributes
esid: sec-date.prototype.getutcdaty
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.getUTCDay;
if (x === 1) {
  Date.prototype.getUTCDay = 2;
} else {
  Date.prototype.getUTCDay = 1;
}

assert.notSameValue(
  Date.prototype.getUTCDay,
  x,
  'The value of Date.prototype.getUTCDay is expected to not equal the value of `x`'
);

// TODO: Convert to verifyProperty() format.
