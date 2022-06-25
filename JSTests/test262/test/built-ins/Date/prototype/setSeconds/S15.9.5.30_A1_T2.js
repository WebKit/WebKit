// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setSeconds" has { DontEnum } attributes
esid: sec-date.prototype.setseconds
description: Checking absence of DontDelete attribute
---*/
assert.notSameValue(
  delete Date.prototype.setSeconds,
  false,
  'The value of delete Date.prototype.setSeconds is not false'
);

assert(
  !Date.prototype.hasOwnProperty('setSeconds'),
  'The value of !Date.prototype.hasOwnProperty(\'setSeconds\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
