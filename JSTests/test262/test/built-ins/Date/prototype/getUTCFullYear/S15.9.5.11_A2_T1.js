// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "getUTCFullYear" is 0
esid: sec-date.prototype.getutcfullyear
description: The "length" property of the "getUTCFullYear" is 0
---*/
assert.sameValue(
  Date.prototype.getUTCFullYear.hasOwnProperty("length"),
  true,
  'Date.prototype.getUTCFullYear.hasOwnProperty("length") must return true'
);

assert.sameValue(
  Date.prototype.getUTCFullYear.length,
  0,
  'The value of Date.prototype.getUTCFullYear.length is expected to be 0'
);
