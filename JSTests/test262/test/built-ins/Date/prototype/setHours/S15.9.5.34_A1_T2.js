// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setHours" has { DontEnum } attributes
esid: sec-date.prototype.sethours
description: Checking absence of DontDelete attribute
---*/
assert.notSameValue(delete Date.prototype.setHours, false, 'The value of delete Date.prototype.setHours is not false');

assert(
  !Date.prototype.hasOwnProperty('setHours'),
  'The value of !Date.prototype.hasOwnProperty(\'setHours\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
