// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setUTCDate property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setutcdate
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.setUTCDate.propertyIsEnumerable('length'),
  'The value of !Date.prototype.setUTCDate.propertyIsEnumerable(\'length\') is expected to be true'
);

for (var x in Date.prototype.setUTCDate) {
  assert.notSameValue(x, "length", 'The value of x is not "length"');
}

// TODO: Convert to verifyProperty() format.
