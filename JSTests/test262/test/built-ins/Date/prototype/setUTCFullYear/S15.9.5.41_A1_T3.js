// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setUTCFullYear" has { DontEnum } attributes
esid: sec-date.prototype.setutcfullyear
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.propertyIsEnumerable('setUTCFullYear'),
  'The value of !Date.prototype.propertyIsEnumerable(\'setUTCFullYear\') is expected to be true'
);

for (var x in Date.prototype) {
  assert.notSameValue(x, "setUTCFullYear", 'The value of x is not "setUTCFullYear"');
}

// TODO: Convert to verifyProperty() format.
