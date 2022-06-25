// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype property "setUTCMilliseconds" has { DontEnum }
    attributes
esid: sec-date.prototype.setutcmilliseconds
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.setUTCMilliseconds;
if (x === 1) {
  Date.prototype.setUTCMilliseconds = 2;
} else {
  Date.prototype.setUTCMilliseconds = 1;
}

assert.notSameValue(
  Date.prototype.setUTCMilliseconds,
  x,
  'The value of Date.prototype.setUTCMilliseconds is expected to not equal the value of `x`'
);

// TODO: Convert to verifyProperty() format.
