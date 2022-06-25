// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getUTCDay" has { DontEnum } attributes
esid: sec-date.prototype.getutcdaty
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.propertyIsEnumerable('getUTCDay'),
  'The value of !Date.prototype.propertyIsEnumerable(\'getUTCDay\') is expected to be true'
);

for (var x in Date.prototype) {
  assert.notSameValue(x, "getUTCDay", 'The value of x is not "getUTCDay"');
}

// TODO: Convert to verifyProperty() format.
