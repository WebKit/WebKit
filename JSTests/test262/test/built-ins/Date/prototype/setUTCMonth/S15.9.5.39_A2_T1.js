// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "setUTCMonth" is 2
esid: sec-date.prototype.setutcmonth
description: The "length" property of the "setUTCMonth" is 2
---*/
assert.sameValue(
  Date.prototype.setUTCMonth.hasOwnProperty("length"),
  true,
  'Date.prototype.setUTCMonth.hasOwnProperty("length") must return true'
);

assert.sameValue(
  Date.prototype.setUTCMonth.length,
  2,
  'The value of Date.prototype.setUTCMonth.length is expected to be 2'
);
