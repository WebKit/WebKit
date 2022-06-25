// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getUTCSeconds" has { DontEnum } attributes
esid: sec-date.prototype.getutcseconds
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.getUTCSeconds;
if (x === 1) {
  Date.prototype.getUTCSeconds = 2;
} else {
  Date.prototype.getUTCSeconds = 1;
}

assert.notSameValue(
  Date.prototype.getUTCSeconds,
  x,
  'The value of Date.prototype.getUTCSeconds is expected to not equal the value of `x`'
);

// TODO: Convert to verifyProperty() format.
