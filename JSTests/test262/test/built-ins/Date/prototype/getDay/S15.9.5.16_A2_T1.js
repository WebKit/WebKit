// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.getday
info: The "length" property of the "getDay" is 0
es5id: 15.9.5.16_A2_T1
description: The "length" property of the "getDay" is 0
---*/
assert.sameValue(
  Date.prototype.getDay.hasOwnProperty("length"),
  true,
  'Date.prototype.getDay.hasOwnProperty("length") must return true'
);

assert.sameValue(Date.prototype.getDay.length, 0, 'The value of Date.prototype.getDay.length is expected to be 0');
