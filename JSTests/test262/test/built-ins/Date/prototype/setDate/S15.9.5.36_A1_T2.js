// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setDate" has { DontEnum } attributes
esid: sec-date.prototype.setdate
description: Checking absence of DontDelete attribute
---*/
assert.notSameValue(delete Date.prototype.setDate, false, 'The value of delete Date.prototype.setDate is not false');

assert(
  !Date.prototype.hasOwnProperty('setDate'),
  'The value of !Date.prototype.hasOwnProperty(\'setDate\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
