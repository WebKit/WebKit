// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setUTCHours" has { DontEnum } attributes
esid: sec-date.prototype.setutchours
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.propertyIsEnumerable('setUTCHours'),
  'The value of !Date.prototype.propertyIsEnumerable(\'setUTCHours\') is expected to be true'
);

for (var x in Date.prototype) {
  assert.notSameValue(x, "setUTCHours", 'The value of x is not "setUTCHours"');
}

// TODO: Convert to verifyProperty() format.
