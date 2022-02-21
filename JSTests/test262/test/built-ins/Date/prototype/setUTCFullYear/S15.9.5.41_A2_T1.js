// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "setUTCFullYear" is 3
esid: sec-date.prototype.setutcfullyear
description: The "length" property of the "setUTCFullYear" is 3
---*/
assert.sameValue(
  Date.prototype.setUTCFullYear.hasOwnProperty("length"),
  true,
  'Date.prototype.setUTCFullYear.hasOwnProperty("length") must return true'
);

assert.sameValue(
  Date.prototype.setUTCFullYear.length,
  3,
  'The value of Date.prototype.setUTCFullYear.length is expected to be 3'
);
