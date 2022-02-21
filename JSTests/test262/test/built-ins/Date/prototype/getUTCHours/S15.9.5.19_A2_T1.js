// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "getUTCHours" is 0
esid: sec-date.prototype.getutchours
description: The "length" property of the "getUTCHours" is 0
---*/
assert.sameValue(
  Date.prototype.getUTCHours.hasOwnProperty("length"),
  true,
  'Date.prototype.getUTCHours.hasOwnProperty("length") must return true'
);

assert.sameValue(
  Date.prototype.getUTCHours.length,
  0,
  'The value of Date.prototype.getUTCHours.length is expected to be 0'
);
