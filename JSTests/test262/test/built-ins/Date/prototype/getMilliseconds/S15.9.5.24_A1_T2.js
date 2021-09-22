// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.getmilliseconds
info: The Date.prototype property "getMilliseconds" has { DontEnum } attributes
es5id: 15.9.5.24_A1_T2
description: Checking absence of DontDelete attribute
---*/
assert.notSameValue(
  delete Date.prototype.getMilliseconds,
  false,
  'The value of delete Date.prototype.getMilliseconds is not false'
);

assert(
  !Date.prototype.hasOwnProperty('getMilliseconds'),
  'The value of !Date.prototype.hasOwnProperty(\'getMilliseconds\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
