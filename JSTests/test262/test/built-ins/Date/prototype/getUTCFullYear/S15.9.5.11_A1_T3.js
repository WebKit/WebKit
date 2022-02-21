// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getUTCFullYear" has { DontEnum } attributes
esid: sec-date.prototype.getutcfullyear
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.propertyIsEnumerable('getUTCFullYear'),
  'The value of !Date.prototype.propertyIsEnumerable(\'getUTCFullYear\') is expected to be true'
);

for (var x in Date.prototype) {
  assert.notSameValue(x, "getUTCFullYear", 'The value of x is not "getUTCFullYear"');
}

// TODO: Convert to verifyProperty() format.
