// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getTime" has { DontEnum } attributes
esid: sec-date.prototype.getseconds
description: Checking absence of DontDelete attribute
---*/
assert.notSameValue(delete Date.prototype.getTime, false, 'The value of delete Date.prototype.getTime is not false');

assert(
  !Date.prototype.hasOwnProperty('getTime'),
  'The value of !Date.prototype.hasOwnProperty(\'getTime\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
