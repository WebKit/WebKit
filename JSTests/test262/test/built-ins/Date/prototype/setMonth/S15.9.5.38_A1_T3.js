// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setMonth" has { DontEnum } attributes
esid: sec-date.prototype.setmonth
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.propertyIsEnumerable('setMonth'),
  'The value of !Date.prototype.propertyIsEnumerable(\'setMonth\') is expected to be true'
);

for (var x in Date.prototype) {
  assert.notSameValue(x, "setMonth", 'The value of x is not "setMonth"');
}

// TODO: Convert to verifyProperty() format.
