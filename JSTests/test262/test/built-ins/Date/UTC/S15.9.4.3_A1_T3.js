// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.utc
info: The Date property "UTC" has { DontEnum } attributes
es5id: 15.9.4.3_A1_T3
description: Checking DontEnum attribute
---*/
assert(
  !Date.propertyIsEnumerable('UTC'),
  'The value of !Date.propertyIsEnumerable(\'UTC\') is expected to be true'
);

for (var x in Date) {
  assert.notSameValue(x, "UTC", 'The value of x is not "UTC"');
}

// TODO: Convert to verifyProperty() format.
