// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype property "setUTCMilliseconds" has { DontEnum }
    attributes
esid: sec-date.prototype.setutcmilliseconds
description: Checking absence of DontDelete attribute
---*/
assert.notSameValue(
  delete Date.prototype.setUTCMilliseconds,
  false,
  'The value of delete Date.prototype.setUTCMilliseconds is not false'
);

assert(
  !Date.prototype.hasOwnProperty('setUTCMilliseconds'),
  'The value of !Date.prototype.hasOwnProperty(\'setUTCMilliseconds\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
