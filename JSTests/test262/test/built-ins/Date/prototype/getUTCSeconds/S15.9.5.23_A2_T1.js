// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "getUTCSeconds" is 0
esid: sec-date.prototype.getutcseconds
description: The "length" property of the "getUTCSeconds" is 0
---*/
assert.sameValue(
  Date.prototype.getUTCSeconds.hasOwnProperty("length"),
  true,
  'Date.prototype.getUTCSeconds.hasOwnProperty("length") must return true'
);

assert.sameValue(
  Date.prototype.getUTCSeconds.length,
  0,
  'The value of Date.prototype.getUTCSeconds.length is expected to be 0'
);
