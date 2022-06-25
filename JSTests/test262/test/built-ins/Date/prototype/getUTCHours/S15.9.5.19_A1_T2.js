// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getUTCHours" has { DontEnum } attributes
esid: sec-date.prototype.getutchours
description: Checking absence of DontDelete attribute
---*/
assert.notSameValue(
  delete Date.prototype.getUTCHours,
  false,
  'The value of delete Date.prototype.getUTCHours is not false'
);

assert(
  !Date.prototype.hasOwnProperty('getUTCHours'),
  'The value of !Date.prototype.hasOwnProperty(\'getUTCHours\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
