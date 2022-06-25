// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.getday
info: The Date.prototype property "getDay" has { DontEnum } attributes
es5id: 15.9.5.16_A1_T3
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.propertyIsEnumerable('getDay'),
  'The value of !Date.prototype.propertyIsEnumerable(\'getDay\') is expected to be true'
);

for (var x in Date.prototype) {
  assert.notSameValue(x, "getDay", 'The value of x is not "getDay"');
}

// TODO: Convert to verifyProperty() format.
