// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "valueOf" has { DontEnum } attributes
esid: sec-date.prototype.valueof
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.propertyIsEnumerable('valueOf'),
  'The value of !Date.prototype.propertyIsEnumerable(\'valueOf\') is expected to be true'
);

for (var x in Date.prototype) {
  assert.notSameValue(x, "valueOf", 'The value of x is not "valueOf"');
}

// TODO: Convert to verifyProperty() format.
