// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.getminutes
info: The Date.prototype property "getMinutes" has { DontEnum } attributes
es5id: 15.9.5.20_A1_T2
description: Checking absence of DontDelete attribute
---*/
assert.notSameValue(
  delete Date.prototype.getMinutes,
  false,
  'The value of delete Date.prototype.getMinutes is not false'
);

assert(
  !Date.prototype.hasOwnProperty('getMinutes'),
  'The value of !Date.prototype.hasOwnProperty(\'getMinutes\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
