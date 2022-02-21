// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "toDateString" has { DontEnum } attributes
esid: sec-date.prototype.todatestring
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.toDateString;
if (x === 1) {
  Date.prototype.toDateString = 2;
} else {
  Date.prototype.toDateString = 1;
}

assert.notSameValue(
  Date.prototype.toDateString,
  x,
  'The value of Date.prototype.toDateString is expected to not equal the value of `x`'
);

// TODO: Convert to verifyProperty() format.
