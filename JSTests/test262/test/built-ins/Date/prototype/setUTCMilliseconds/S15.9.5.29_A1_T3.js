// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype property "setUTCMilliseconds" has { DontEnum }
    attributes
esid: sec-date.prototype.setutcmilliseconds
description: Checking DontEnum attribute
---*/
assert(
  !Date.prototype.propertyIsEnumerable('setUTCMilliseconds'),
  'The value of !Date.prototype.propertyIsEnumerable(\'setUTCMilliseconds\') is expected to be true'
);

for (var x in Date.prototype) {
  assert.notSameValue(x, "setUTCMilliseconds", 'The value of x is not "setUTCMilliseconds"');
}

// TODO: Convert to verifyProperty() format.
