// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype property "getUTCMilliseconds" has { DontEnum }
    attributes
esid: sec-date.prototype.getutcmilliseconds
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.getUTCMilliseconds;
if (x === 1) {
  Date.prototype.getUTCMilliseconds = 2;
} else {
  Date.prototype.getUTCMilliseconds = 1;
}

assert.notSameValue(
  Date.prototype.getUTCMilliseconds,
  x,
  'The value of Date.prototype.getUTCMilliseconds is expected to not equal the value of `x`'
);

// TODO: Convert to verifyProperty() format.
