// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getDate" has { DontEnum } attributes
esid: sec-date.prototype.getdate
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.propertyIsEnumerable('getDate'),
  'The value of !Date.prototype.propertyIsEnumerable(\'getDate\') is expected to be true'
);

for (var x in Date.prototype) {
  assert.notSameValue(x, "getDate", 'The value of x is not "getDate"');
}

// TODO: Convert to verifyProperty() format.
