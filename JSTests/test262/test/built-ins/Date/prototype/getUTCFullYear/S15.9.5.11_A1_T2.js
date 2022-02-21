// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getUTCFullYear" has { DontEnum } attributes
esid: sec-date.prototype.getutcfullyear
description: Checking absence of DontDelete attribute
---*/
assert.notSameValue(
  delete Date.prototype.getUTCFullYear,
  false,
  'The value of delete Date.prototype.getUTCFullYear is not false'
);

assert(
  !Date.prototype.hasOwnProperty('getUTCFullYear'),
  'The value of !Date.prototype.hasOwnProperty(\'getUTCFullYear\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
