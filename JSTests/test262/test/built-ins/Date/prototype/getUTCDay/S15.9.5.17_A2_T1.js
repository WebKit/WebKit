// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "getUTCDay" is 0
esid: sec-date.prototype.getutcdaty
description: The "length" property of the "getUTCDay" is 0
---*/
assert.sameValue(
  Date.prototype.getUTCDay.hasOwnProperty("length"),
  true,
  'Date.prototype.getUTCDay.hasOwnProperty("length") must return true'
);

assert.sameValue(
  Date.prototype.getUTCDay.length,
  0,
  'The value of Date.prototype.getUTCDay.length is expected to be 0'
);
