// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date property "prototype" has { DontEnum, DontDelete, ReadOnly }
    attributes
esid: sec-date.prototype
description: Checking DontEnum attribute
---*/
assert(
  !Date.propertyIsEnumerable('prototype'),
  'The value of !Date.propertyIsEnumerable(\'prototype\') is expected to be true'
);

for (var x in Date) {
  assert.notSameValue(x, "prototype", 'The value of x is not "prototype"');
}

// TODO: Convert to verifyProperty() format.
