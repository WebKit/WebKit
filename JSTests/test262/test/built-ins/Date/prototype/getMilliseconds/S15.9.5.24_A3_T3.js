// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.getmilliseconds
info: |
    The Date.prototype.getMilliseconds property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
es5id: 15.9.5.24_A3_T3
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.getMilliseconds.propertyIsEnumerable('length'),
  'The value of !Date.prototype.getMilliseconds.propertyIsEnumerable(\'length\') is expected to be true'
);

for (var x in Date.prototype.getMilliseconds) {
  assert.notSameValue(x, "length", 'The value of x is not "length"');
}

// TODO: Convert to verifyProperty() format.
