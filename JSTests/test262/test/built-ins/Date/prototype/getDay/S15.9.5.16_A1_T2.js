// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.getday
info: The Date.prototype property "getDay" has { DontEnum } attributes
es5id: 15.9.5.16_A1_T2
description: Checking absence of DontDelete attribute
---*/
assert.notSameValue(delete Date.prototype.getDay, false, 'The value of delete Date.prototype.getDay is not false');

assert(
  !Date.prototype.hasOwnProperty('getDay'),
  'The value of !Date.prototype.hasOwnProperty(\'getDay\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
