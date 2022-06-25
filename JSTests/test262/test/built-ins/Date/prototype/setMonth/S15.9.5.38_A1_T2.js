// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setMonth" has { DontEnum } attributes
esid: sec-date.prototype.setmonth
description: Checking absence of DontDelete attribute
---*/
assert.notSameValue(delete Date.prototype.setMonth, false, 'The value of delete Date.prototype.setMonth is not false');

assert(
  !Date.prototype.hasOwnProperty('setMonth'),
  'The value of !Date.prototype.hasOwnProperty(\'setMonth\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
