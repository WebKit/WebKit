// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "getMonth" is 0
esid: sec-date.prototype.getmonth
description: The "length" property of the "getMonth" is 0
---*/
assert.sameValue(
  Date.prototype.getMonth.hasOwnProperty("length"),
  true,
  'Date.prototype.getMonth.hasOwnProperty("length") must return true'
);

assert.sameValue(Date.prototype.getMonth.length, 0, 'The value of Date.prototype.getMonth.length is expected to be 0');
