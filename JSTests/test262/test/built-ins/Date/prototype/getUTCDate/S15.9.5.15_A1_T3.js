// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getUTCDate" has { DontEnum } attributes
esid: sec-date.prototype.getutcdate
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.propertyIsEnumerable('getUTCDate'),
  'The value of !Date.prototype.propertyIsEnumerable(\'getUTCDate\') is expected to be true'
);

for (var x in Date.prototype) {
  assert.notSameValue(x, "getUTCDate", 'The value of x is not "getUTCDate"');
}

// TODO: Convert to verifyProperty() format.
