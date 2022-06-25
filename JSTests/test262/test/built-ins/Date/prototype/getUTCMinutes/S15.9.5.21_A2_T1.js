// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "getUTCMinutes" is 0
esid: sec-date.prototype.getutcminutes
description: The "length" property of the "getUTCMinutes" is 0
---*/
assert.sameValue(
  Date.prototype.getUTCMinutes.hasOwnProperty("length"),
  true,
  'Date.prototype.getUTCMinutes.hasOwnProperty("length") must return true'
);

assert.sameValue(
  Date.prototype.getUTCMinutes.length,
  0,
  'The value of Date.prototype.getUTCMinutes.length is expected to be 0'
);
