// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getMonth" has { DontEnum } attributes
esid: sec-date.prototype.getmonth
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.propertyIsEnumerable('getMonth'),
  'The value of !Date.prototype.propertyIsEnumerable(\'getMonth\') is expected to be true'
);

for (var x in Date.prototype) {
  assert.notSameValue(x, "getMonth", 'The value of x is not "getMonth"');
}

// TODO: Convert to verifyProperty() format.
