// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.gethours
info: The Date.prototype property "getHours" has { DontEnum } attributes
es5id: 15.9.5.18_A1_T3
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.propertyIsEnumerable('getHours'),
  'The value of !Date.prototype.propertyIsEnumerable(\'getHours\') is expected to be true'
);

for (var x in Date.prototype) {
  assert.notSameValue(x, "getHours", 'The value of x is not "getHours"');
}

// TODO: Convert to verifyProperty() format.
