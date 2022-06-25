// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.getminutes
info: The "length" property of the "getMinutes" is 0
es5id: 15.9.5.20_A2_T1
description: The "length" property of the "getMinutes" is 0
---*/
assert.sameValue(
  Date.prototype.getMinutes.hasOwnProperty("length"),
  true,
  'Date.prototype.getMinutes.hasOwnProperty("length") must return true'
);

assert.sameValue(
  Date.prototype.getMinutes.length,
  0,
  'The value of Date.prototype.getMinutes.length is expected to be 0'
);
