// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.getfullyear
info: The Date.prototype property "getFullYear" has { DontEnum } attributes
es5id: 15.9.5.10_A1_T3
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.propertyIsEnumerable('getFullYear'),
  'The value of !Date.prototype.propertyIsEnumerable(\'getFullYear\') is expected to be true'
);

for (var x in Date.prototype) {
  assert.notSameValue(x, "getFullYear", 'The value of x is not "getFullYear"');
}

// TODO: Convert to verifyProperty() format.
