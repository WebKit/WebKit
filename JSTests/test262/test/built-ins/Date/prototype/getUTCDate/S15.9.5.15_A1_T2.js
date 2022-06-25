// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getUTCDate" has { DontEnum } attributes
esid: sec-date.prototype.getutcdate
description: Checking absence of DontDelete attribute
---*/
assert.notSameValue(
  delete Date.prototype.getUTCDate,
  false,
  'The value of delete Date.prototype.getUTCDate is not false'
);

assert(
  !Date.prototype.hasOwnProperty('getUTCDate'),
  'The value of !Date.prototype.hasOwnProperty(\'getUTCDate\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
