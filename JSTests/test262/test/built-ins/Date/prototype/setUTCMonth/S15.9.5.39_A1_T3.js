// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setUTCMonth" has { DontEnum } attributes
esid: sec-date.prototype.setutcmonth
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.propertyIsEnumerable('setUTCMonth'),
  'The value of !Date.prototype.propertyIsEnumerable(\'setUTCMonth\') is expected to be true'
);

for (var x in Date.prototype) {
  assert.notSameValue(x, "setUTCMonth", 'The value of x is not "setUTCMonth"');
}

// TODO: Convert to verifyProperty() format.
