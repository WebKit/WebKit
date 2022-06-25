// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setUTCDate" has { DontEnum } attributes
esid: sec-date.prototype.setutcdate
description: Checking absence of DontDelete attribute
---*/
assert.notSameValue(
  delete Date.prototype.setUTCDate,
  false,
  'The value of delete Date.prototype.setUTCDate is not false'
);

assert(
  !Date.prototype.hasOwnProperty('setUTCDate'),
  'The value of !Date.prototype.hasOwnProperty(\'setUTCDate\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
