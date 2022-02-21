// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype property "toLocaleDateString" has { DontEnum }
    attributes
esid: sec-date.prototype.tolocaledatestring
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.propertyIsEnumerable('toLocaleDateString'),
  'The value of !Date.prototype.propertyIsEnumerable(\'toLocaleDateString\') is expected to be true'
);

for (var x in Date.prototype) {
  assert.notSameValue(x, "toLocaleDateString", 'The value of x is not "toLocaleDateString"');
}

// TODO: Convert to verifyProperty() format.
