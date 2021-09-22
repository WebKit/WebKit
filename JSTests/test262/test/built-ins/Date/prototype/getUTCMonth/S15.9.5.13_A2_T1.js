// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "getUTCMonth" is 0
esid: sec-date.prototype.getutcmonth
description: The "length" property of the "getUTCMonth" is 0
---*/
assert.sameValue(
  Date.prototype.getUTCMonth.hasOwnProperty("length"),
  true,
  'Date.prototype.getUTCMonth.hasOwnProperty("length") must return true'
);

assert.sameValue(
  Date.prototype.getUTCMonth.length,
  0,
  'The value of Date.prototype.getUTCMonth.length is expected to be 0'
);
