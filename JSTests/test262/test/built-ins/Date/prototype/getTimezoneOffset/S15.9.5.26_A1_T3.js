// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype property "getTimezoneOffset" has { DontEnum }
    attributes
esid: sec-date.prototype.gettimezoneoffset
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.propertyIsEnumerable('getTimezoneOffset'),
  'The value of !Date.prototype.propertyIsEnumerable(\'getTimezoneOffset\') is expected to be true'
);

for (var x in Date.prototype) {
  assert.notSameValue(x, "getTimezoneOffset", 'The value of x is not "getTimezoneOffset"');
}

// TODO: Convert to verifyProperty() format.
