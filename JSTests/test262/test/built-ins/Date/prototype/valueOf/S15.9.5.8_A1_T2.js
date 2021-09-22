// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "valueOf" has { DontEnum } attributes
esid: sec-date.prototype.valueof
description: Checking absence of DontDelete attribute
---*/

assert.notSameValue(delete Date.prototype.valueOf, false, 'The value of delete Date.prototype.valueOf is not false');

assert(
  !Date.prototype.hasOwnProperty('valueOf'),
  'The value of !Date.prototype.hasOwnProperty(\'valueOf\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
