// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.getfullyear
info: The "length" property of the "getFullYear" is 0
es5id: 15.9.5.10_A2_T1
description: The "length" property of the "getFullYear" is 0
---*/
assert.sameValue(
  Date.prototype.getFullYear.hasOwnProperty("length"),
  true,
  'Date.prototype.getFullYear.hasOwnProperty("length") must return true'
);

assert.sameValue(
  Date.prototype.getFullYear.length,
  0,
  'The value of Date.prototype.getFullYear.length is expected to be 0'
);
