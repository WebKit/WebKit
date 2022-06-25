// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getSeconds" has { DontEnum } attributes
esid: sec-date.prototype.getseconds
description: Checking absence of DontDelete attribute
---*/
assert.notSameValue(
  delete Date.prototype.getSeconds,
  false,
  'The value of delete Date.prototype.getSeconds is not false'
);

assert(
  !Date.prototype.hasOwnProperty('getSeconds'),
  'The value of !Date.prototype.hasOwnProperty(\'getSeconds\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
