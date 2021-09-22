// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setUTCHours" has { DontEnum } attributes
esid: sec-date.prototype.setutchours
description: Checking absence of DontDelete attribute
---*/
assert.notSameValue(
  delete Date.prototype.setUTCHours,
  false,
  'The value of delete Date.prototype.setUTCHours is not false'
);

assert(
  !Date.prototype.hasOwnProperty('setUTCHours'),
  'The value of !Date.prototype.hasOwnProperty(\'setUTCHours\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
