// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "toUTCString" has { DontEnum } attributes
esid: sec-date.prototype.toutcstring
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.propertyIsEnumerable('toUTCString'),
  'The value of !Date.prototype.propertyIsEnumerable(\'toUTCString\') is expected to be true'
);

for (var x in Date.prototype) {
  assert.notSameValue(x, "toUTCString", 'The value of x is not "toUTCString"');
}

// TODO: Convert to verifyProperty() format.
