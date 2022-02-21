// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setDate" has { DontEnum } attributes
esid: sec-date.prototype.setdate
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.propertyIsEnumerable('setDate'),
  'The value of !Date.prototype.propertyIsEnumerable(\'setDate\') is expected to be true'
);

for (var x in Date.prototype) {
  assert.notSameValue(x, "setDate", 'The value of x is not "setDate"');
}

// TODO: Convert to verifyProperty() format.
