// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype property "toLocaleTimeString" has { DontEnum }
    attributes
esid: sec-date.prototype.tolocaletimestring
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.propertyIsEnumerable('toLocaleTimeString'),
  'The value of !Date.prototype.propertyIsEnumerable(\'toLocaleTimeString\') is expected to be true'
);

for (var x in Date.prototype) {
  assert.notSameValue(x, "toLocaleTimeString", 'The value of x is not "toLocaleTimeString"');
}

// TODO: Convert to verifyProperty() format.
