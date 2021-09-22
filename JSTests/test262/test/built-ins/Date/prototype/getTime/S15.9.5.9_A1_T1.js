// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getTime" has { DontEnum } attributes
esid: sec-date.prototype.getseconds
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.getTime;
if (x === 1) {
  Date.prototype.getTime = 2;
} else {
  Date.prototype.getTime = 1;
}

assert.notSameValue(
  Date.prototype.getTime,
  x,
  'The value of Date.prototype.getTime is expected to not equal the value of `x`'
);

// TODO: Convert to verifyProperty() format.
