// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setUTCMonth" has { DontEnum } attributes
esid: sec-date.prototype.setutcmonth
description: Checking absence of DontDelete attribute
---*/
assert.notSameValue(
  delete Date.prototype.setUTCMonth,
  false,
  'The value of delete Date.prototype.setUTCMonth is not false'
);

assert(
  !Date.prototype.hasOwnProperty('setUTCMonth'),
  'The value of !Date.prototype.hasOwnProperty(\'setUTCMonth\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
