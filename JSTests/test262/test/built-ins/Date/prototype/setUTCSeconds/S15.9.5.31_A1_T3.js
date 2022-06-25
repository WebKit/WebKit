// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setUTCSeconds" has { DontEnum } attributes
esid: sec-date.prototype.setutcseconds
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.propertyIsEnumerable('setUTCSeconds'),
  'The value of !Date.prototype.propertyIsEnumerable(\'setUTCSeconds\') is expected to be true'
);

for (var x in Date.prototype) {
  assert.notSameValue(x, "setUTCSeconds", 'The value of x is not "setUTCSeconds"');
}

// TODO: Convert to verifyProperty() format.
