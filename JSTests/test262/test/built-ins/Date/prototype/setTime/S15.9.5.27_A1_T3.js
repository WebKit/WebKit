// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setTime" has { DontEnum } attributes
esid: sec-date.prototype.settime
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.propertyIsEnumerable('setTime'),
  'The value of !Date.prototype.propertyIsEnumerable(\'setTime\') is expected to be true'
);

for (var x in Date.prototype) {
  assert.notSameValue(x, "setTime", 'The value of x is not "setTime"');
}

// TODO: Convert to verifyProperty() format.
