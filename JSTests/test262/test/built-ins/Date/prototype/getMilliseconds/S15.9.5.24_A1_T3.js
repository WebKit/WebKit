// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.getmilliseconds
info: The Date.prototype property "getMilliseconds" has { DontEnum } attributes
es5id: 15.9.5.24_A1_T3
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.propertyIsEnumerable('getMilliseconds'),
  'The value of !Date.prototype.propertyIsEnumerable(\'getMilliseconds\') is expected to be true'
);

for (var x in Date.prototype) {
  assert.notSameValue(x, "getMilliseconds", 'The value of x is not "getMilliseconds"');
}

// TODO: Convert to verifyProperty() format.
