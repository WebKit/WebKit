// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getSeconds" has { DontEnum } attributes
esid: sec-date.prototype.getseconds
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.propertyIsEnumerable('getSeconds'),
  'The value of !Date.prototype.propertyIsEnumerable(\'getSeconds\') is expected to be true'
);

for (var x in Date.prototype) {
  assert.notSameValue(x, "getSeconds", 'The value of x is not "getSeconds"');
}

// TODO: Convert to verifyProperty() format.
