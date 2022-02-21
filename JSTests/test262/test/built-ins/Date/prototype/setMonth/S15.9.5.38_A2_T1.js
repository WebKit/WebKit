// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "setMonth" is 2
esid: sec-date.prototype.setmonth
description: The "length" property of the "setMonth" is 2
---*/
assert.sameValue(
  Date.prototype.setMonth.hasOwnProperty("length"),
  true,
  'Date.prototype.setMonth.hasOwnProperty("length") must return true'
);

assert.sameValue(Date.prototype.setMonth.length, 2, 'The value of Date.prototype.setMonth.length is expected to be 2');
