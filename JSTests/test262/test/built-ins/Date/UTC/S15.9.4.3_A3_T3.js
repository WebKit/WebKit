// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.utc
info: |
    The Date.UTC property "length" has { ReadOnly, DontDelete, DontEnum }
    attributes
es5id: 15.9.4.3_A3_T3
description: Checking DontEnum attribute
---*/
assert(
  !Date.UTC.propertyIsEnumerable('length'),
  'The value of !Date.UTC.propertyIsEnumerable(\'length\') is expected to be true'
);

for (var x in Date.UTC) {
  assert.notSameValue(x, "length", 'The value of x is not "length"');
}

// TODO: Convert to verifyProperty() format.
