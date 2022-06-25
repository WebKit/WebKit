// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setUTCMilliseconds property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setutcmilliseconds
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.setUTCMilliseconds.propertyIsEnumerable('length'),
  'The value of !Date.prototype.setUTCMilliseconds.propertyIsEnumerable(\'length\') is expected to be true'
);

for (var x in Date.prototype.setUTCMilliseconds) {
  assert.notSameValue(x, "length", 'The value of x is not "length"');
}

// TODO: Convert to verifyProperty() format.
