// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getUTCHours" has { DontEnum } attributes
esid: sec-date.prototype.getutchours
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.propertyIsEnumerable('getUTCHours'),
  'The value of !Date.prototype.propertyIsEnumerable(\'getUTCHours\') is expected to be true'
);

for (var x in Date.prototype) {
  assert.notSameValue(x, "getUTCHours", 'The value of x is not "getUTCHours"');
}

// TODO: Convert to verifyProperty() format.
