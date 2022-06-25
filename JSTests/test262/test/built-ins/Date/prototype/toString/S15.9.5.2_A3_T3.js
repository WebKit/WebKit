// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.toString property "length" has { ReadOnly, DontDelete,
    DontEnum } attributes
esid: sec-date.prototype.tostring
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.toString.propertyIsEnumerable('length'),
  'The value of !Date.prototype.toString.propertyIsEnumerable(\'length\') is expected to be true'
);

for (var x in Date.prototype.toString) {
  assert.notSameValue(x, "length", 'The value of x is not "length"');
}

// TODO: Convert to verifyProperty() format.
