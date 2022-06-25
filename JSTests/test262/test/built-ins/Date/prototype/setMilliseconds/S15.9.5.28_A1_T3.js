// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setMilliseconds" has { DontEnum } attributes
esid: sec-date.prototype.setmilliseconds
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.propertyIsEnumerable('setMilliseconds'),
  'The value of !Date.prototype.propertyIsEnumerable(\'setMilliseconds\') is expected to be true'
);

for (var x in Date.prototype) {
  assert.notSameValue(x, "setMilliseconds", 'The value of x is not "setMilliseconds"');
}

// TODO: Convert to verifyProperty() format.
