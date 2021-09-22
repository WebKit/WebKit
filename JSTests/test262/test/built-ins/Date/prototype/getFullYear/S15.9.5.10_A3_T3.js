// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.getfullyear
info: |
    The Date.prototype.getFullYear property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
es5id: 15.9.5.10_A3_T3
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.getFullYear.propertyIsEnumerable('length'),
  'The value of !Date.prototype.getFullYear.propertyIsEnumerable(\'length\') is expected to be true'
);

for (var x in Date.prototype.getFullYear) {
  assert.notSameValue(x, "length", 'The value of x is not "length"');
}

// TODO: Convert to verifyProperty() format.
