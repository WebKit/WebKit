// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getUTCMonth" has { DontEnum } attributes
esid: sec-date.prototype.getutcmonth
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.propertyIsEnumerable('getUTCMonth'),
  'The value of !Date.prototype.propertyIsEnumerable(\'getUTCMonth\') is expected to be true'
);

for (var x in Date.prototype) {
  assert.notSameValue(x, "getUTCMonth", 'The value of x is not "getUTCMonth"');
}

// TODO: Convert to verifyProperty() format.
