// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setSeconds" has { DontEnum } attributes
esid: sec-date.prototype.setseconds
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.propertyIsEnumerable('setSeconds'),
  'The value of !Date.prototype.propertyIsEnumerable(\'setSeconds\') is expected to be true'
);

for (var x in Date.prototype) {
  assert.notSameValue(x, "setSeconds", 'The value of x is not "setSeconds"');
}

// TODO: Convert to verifyProperty() format.
