// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype property "toLocaleTimeString" has { DontEnum }
    attributes
esid: sec-date.prototype.tolocaletimestring
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.toLocaleTimeString;
if (x === 1) {
  Date.prototype.toLocaleTimeString = 2;
} else {
  Date.prototype.toLocaleTimeString = 1;
}

assert.notSameValue(
  Date.prototype.toLocaleTimeString,
  x,
  'The value of Date.prototype.toLocaleTimeString is expected to not equal the value of `x`'
);

// TODO: Convert to verifyProperty() format.
