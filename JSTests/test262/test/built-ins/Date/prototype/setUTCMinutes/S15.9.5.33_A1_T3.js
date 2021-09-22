// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setUTCMinutes" has { DontEnum } attributes
esid: sec-date.prototype.setutcminutes
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.propertyIsEnumerable('setUTCMinutes'),
  'The value of !Date.prototype.propertyIsEnumerable(\'setUTCMinutes\') is expected to be true'
);

for (var x in Date.prototype) {
  assert.notSameValue(x, "setUTCMinutes", 'The value of x is not "setUTCMinutes"');
}

// TODO: Convert to verifyProperty() format.
