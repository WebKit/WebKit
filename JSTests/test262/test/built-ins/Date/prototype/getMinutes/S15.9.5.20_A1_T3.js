// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.getminutes
info: The Date.prototype property "getMinutes" has { DontEnum } attributes
es5id: 15.9.5.20_A1_T3
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.propertyIsEnumerable('getMinutes'),
  'The value of !Date.prototype.propertyIsEnumerable(\'getMinutes\') is expected to be true'
);

for (var x in Date.prototype) {
  assert.notSameValue(x, "getMinutes", 'The value of x is not "getMinutes"');
}

// TODO: Convert to verifyProperty() format.
