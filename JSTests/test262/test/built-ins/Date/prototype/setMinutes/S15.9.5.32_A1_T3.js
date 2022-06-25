// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setMinutes" has { DontEnum } attributes
esid: sec-date.prototype.setminutes
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.propertyIsEnumerable('setMinutes'),
  'The value of !Date.prototype.propertyIsEnumerable(\'setMinutes\') is expected to be true'
);

for (var x in Date.prototype) {
  assert.notSameValue(x, "setMinutes", 'The value of x is not "setMinutes"');
}

// TODO: Convert to verifyProperty() format.
