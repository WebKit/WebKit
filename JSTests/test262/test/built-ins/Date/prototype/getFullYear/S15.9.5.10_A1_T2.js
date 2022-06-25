// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.getfullyear
info: The Date.prototype property "getFullYear" has { DontEnum } attributes
es5id: 15.9.5.10_A1_T2
description: Checking absence of DontDelete attribute
---*/
assert.notSameValue(
  delete Date.prototype.getFullYear,
  false,
  'The value of delete Date.prototype.getFullYear is not false'
);

assert(
  !Date.prototype.hasOwnProperty('getFullYear'),
  'The value of !Date.prototype.hasOwnProperty(\'getFullYear\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
