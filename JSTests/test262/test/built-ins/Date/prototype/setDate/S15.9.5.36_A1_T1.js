// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setDate" has { DontEnum } attributes
esid: sec-date.prototype.setdate
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.setDate;
if (x === 1) {
  Date.prototype.setDate = 2;
} else {
  Date.prototype.setDate = 1;
}

assert.notSameValue(
  Date.prototype.setDate,
  x,
  'The value of Date.prototype.setDate is expected to not equal the value of `x`'
);

// TODO: Convert to verifyProperty() format.
