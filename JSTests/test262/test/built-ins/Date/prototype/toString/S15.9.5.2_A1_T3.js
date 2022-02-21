// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "toString" has { DontEnum } attributes
esid: sec-date.prototype.tostring
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.propertyIsEnumerable('toString'),
  'The value of !Date.prototype.propertyIsEnumerable(\'toString\') is expected to be true'
);

for (var x in Date.prototype) {
  assert.notSameValue(x, "toString", 'The value of x is not "toString"');
}

// TODO: Convert to verifyProperty() format.
