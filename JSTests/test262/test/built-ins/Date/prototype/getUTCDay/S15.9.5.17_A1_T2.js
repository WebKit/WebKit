// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getUTCDay" has { DontEnum } attributes
esid: sec-date.prototype.getutcdaty
description: Checking absence of DontDelete attribute
---*/
assert.notSameValue(delete Date.prototype.getUTCDay, false, 'The value of delete Date.prototype.getUTCDay is not false');

assert(
  !Date.prototype.hasOwnProperty('getUTCDay'),
  'The value of !Date.prototype.hasOwnProperty(\'getUTCDay\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
