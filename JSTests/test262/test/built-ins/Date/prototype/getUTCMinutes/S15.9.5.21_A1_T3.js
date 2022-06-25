// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getUTCMinutes" has { DontEnum } attributes
esid: sec-date.prototype.getutcminutes
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.propertyIsEnumerable('getUTCMinutes'),
  'The value of !Date.prototype.propertyIsEnumerable(\'getUTCMinutes\') is expected to be true'
);

for (var x in Date.prototype) {
  assert.notSameValue(x, "getUTCMinutes", 'The value of x is not "getUTCMinutes"');
}

// TODO: Convert to verifyProperty() format.
