// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setFullYear" has { DontEnum } attributes
esid: sec-date.prototype.setfullyear
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.propertyIsEnumerable('setFullYear'),
  'The value of !Date.prototype.propertyIsEnumerable(\'setFullYear\') is expected to be true'
);

for (var x in Date.prototype) {
  assert.notSameValue(x, "setFullYear", 'The value of x is not "setFullYear"');
}

// TODO: Convert to verifyProperty() format.
