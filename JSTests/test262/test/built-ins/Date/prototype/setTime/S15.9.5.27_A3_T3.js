// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setTime property "length" has { ReadOnly, DontDelete,
    DontEnum } attributes
esid: sec-date.prototype.settime
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.setTime.propertyIsEnumerable('length'),
  'The value of !Date.prototype.setTime.propertyIsEnumerable(\'length\') is expected to be true'
);

for (var x in Date.prototype.setTime) {
  assert.notSameValue(x, "length", 'The value of x is not "length"');
}

// TODO: Convert to verifyProperty() format.
