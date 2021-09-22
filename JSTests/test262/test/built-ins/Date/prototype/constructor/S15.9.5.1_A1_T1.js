// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "constructor" has { DontEnum } attributes
esid: sec-date.prototype.constructor
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.constructor;
if (x === 1) {
  Date.prototype.constructor = 2;
} else {
  Date.prototype.constructor = 1;
}

assert.notSameValue(
  Date.prototype.constructor,
  x,
  'The value of Date.prototype.constructor is expected to not equal the value of `x`'
);

// TODO: Convert to verifyProperty() format.
