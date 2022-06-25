// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.gethours
info: The Date.prototype property "getHours" has { DontEnum } attributes
es5id: 15.9.5.18_A1_T2
description: Checking absence of DontDelete attribute
---*/
assert.notSameValue(delete Date.prototype.getHours, false, 'The value of delete Date.prototype.getHours is not false');

assert(
  !Date.prototype.hasOwnProperty('getHours'),
  'The value of !Date.prototype.hasOwnProperty(\'getHours\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
