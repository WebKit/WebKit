// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype property "getMonth" has { DontEnum } attributes
esid: sec-date.prototype.getmonth
description: Checking absence of DontDelete attribute
---*/
assert.notSameValue(delete Date.prototype.getMonth, false, 'The value of delete Date.prototype.getMonth is not false');

assert(
  !Date.prototype.hasOwnProperty('getMonth'),
  'The value of !Date.prototype.hasOwnProperty(\'getMonth\') is expected to be true'
);

// TODO: Convert to verifyProperty() format.
