// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getUTCSeconds" has { DontEnum } attributes
esid: sec-date.prototype.getutcseconds
description: Checking absence of DontDelete attribute
---*/
assert.notSameValue(
  delete Date.prototype.getUTCSeconds,
  false,
  'The value of delete Date.prototype.getUTCSeconds is not false'
);

assert(
  !Date.prototype.hasOwnProperty('getUTCSeconds'),
  'The value of !Date.prototype.hasOwnProperty(\'getUTCSeconds\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
