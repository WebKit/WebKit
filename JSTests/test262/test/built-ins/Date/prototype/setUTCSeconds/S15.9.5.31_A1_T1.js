// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setUTCSeconds" has { DontEnum } attributes
esid: sec-date.prototype.setutcseconds
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.setUTCSeconds;
if (x === 1) {
  Date.prototype.setUTCSeconds = 2;
} else {
  Date.prototype.setUTCSeconds = 1;
}

assert.notSameValue(
  Date.prototype.setUTCSeconds,
  x,
  'The value of Date.prototype.setUTCSeconds is expected to not equal the value of `x`'
);

// TODO: Convert to verifyProperty() format.
