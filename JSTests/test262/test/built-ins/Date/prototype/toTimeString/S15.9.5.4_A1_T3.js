// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "toTimeString" has { DontEnum } attributes
esid: sec-date.prototype.totimestring
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.propertyIsEnumerable('toTimeString'),
  'The value of !Date.prototype.propertyIsEnumerable(\'toTimeString\') is expected to be true'
);

for (var x in Date.prototype) {
  assert.notSameValue(x, "toTimeString", 'The value of x is not "toTimeString"');
}

// TODO: Convert to verifyProperty() format.
