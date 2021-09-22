// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.gethours
info: The "length" property of the "getHours" is 0
es5id: 15.9.5.18_A2_T1
description: The "length" property of the "getHours" is 0
---*/
assert.sameValue(
  Date.prototype.getHours.hasOwnProperty("length"),
  true,
  'Date.prototype.getHours.hasOwnProperty("length") must return true'
);

assert.sameValue(Date.prototype.getHours.length, 0, 'The value of Date.prototype.getHours.length is expected to be 0');
