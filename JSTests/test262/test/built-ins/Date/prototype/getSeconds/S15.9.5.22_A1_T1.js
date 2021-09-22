// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getSeconds" has { DontEnum } attributes
esid: sec-date.prototype.getseconds
description: Checking absence of ReadOnly attribute
---*/

var x = Date.prototype.getSeconds;
if (x === 1) {
  Date.prototype.getSeconds = 2;
} else {
  Date.prototype.getSeconds = 1;
}

assert.notSameValue(
  Date.prototype.getSeconds,
  x,
  'The value of Date.prototype.getSeconds is expected to not equal the value of `x`'
);

// TODO: Convert to verifyProperty() format.
