// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setUTCHours property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setutchours
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.setUTCHours.propertyIsEnumerable('length'),
  'The value of !Date.prototype.setUTCHours.propertyIsEnumerable(\'length\') is expected to be true'
);

for (var x in Date.prototype.setUTCHours) {
  assert.notSameValue(x, "length", 'The value of x is not "length"');
}

// TODO: Convert to verifyProperty() format.
