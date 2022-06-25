// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getUTCMonth" has { DontEnum } attributes
esid: sec-date.prototype.getutcmonth
description: Checking absence of DontDelete attribute
---*/
assert.notSameValue(
  delete Date.prototype.getUTCMonth,
  false,
  'The value of delete Date.prototype.getUTCMonth is not false'
);

assert(
  !Date.prototype.hasOwnProperty('getUTCMonth'),
  'The value of !Date.prototype.hasOwnProperty(\'getUTCMonth\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
