// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "getUTCDate" is 0
esid: sec-date.prototype.getutcdate
description: The "length" property of the "getUTCDate" is 0
---*/
assert.sameValue(
  Date.prototype.getUTCDate.hasOwnProperty("length"),
  true,
  'Date.prototype.getUTCDate.hasOwnProperty("length") must return true'
);

assert.sameValue(
  Date.prototype.getUTCDate.length,
  0,
  'The value of Date.prototype.getUTCDate.length is expected to be 0'
);
