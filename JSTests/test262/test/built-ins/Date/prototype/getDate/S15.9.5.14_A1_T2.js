// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getDate" has { DontEnum } attributes
esid: sec-date.prototype.getdate
description: Checking absence of DontDelete attribute
---*/
assert.notSameValue(delete Date.prototype.getDate, false, 'The value of delete Date.prototype.getDate is not false');

assert(
  !Date.prototype.hasOwnProperty('getDate'),
  'The value of !Date.prototype.hasOwnProperty(\'getDate\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
