// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype property "getUTCMilliseconds" has { DontEnum }
    attributes
esid: sec-date.prototype.getutcmilliseconds
description: Checking absence of DontDelete attribute
---*/
assert.notSameValue(
  delete Date.prototype.getUTCMilliseconds,
  false,
  'The value of delete Date.prototype.getUTCMilliseconds is not false'
);

assert(
  !Date.prototype.hasOwnProperty('getUTCMilliseconds'),
  'The value of !Date.prototype.hasOwnProperty(\'getUTCMilliseconds\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
