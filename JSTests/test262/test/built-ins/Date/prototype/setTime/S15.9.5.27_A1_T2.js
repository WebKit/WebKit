// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "setTime" has { DontEnum } attributes
esid: sec-date.prototype.settime
description: Checking absence of DontDelete attribute
---*/
assert.notSameValue(delete Date.prototype.setTime, false, 'The value of delete Date.prototype.setTime is not false');

assert(
  !Date.prototype.hasOwnProperty('setTime'),
  'The value of !Date.prototype.hasOwnProperty(\'setTime\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
