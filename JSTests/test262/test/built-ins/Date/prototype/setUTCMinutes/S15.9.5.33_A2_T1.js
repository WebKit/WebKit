// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "setUTCMinutes" is 3
esid: sec-date.prototype.setutcminutes
description: The "length" property of the "setUTCMinutes" is 3
---*/
assert.sameValue(
  Date.prototype.setUTCMinutes.hasOwnProperty("length"),
  true,
  'Date.prototype.setUTCMinutes.hasOwnProperty("length") must return true'
);

assert.sameValue(
  Date.prototype.setUTCMinutes.length,
  3,
  'The value of Date.prototype.setUTCMinutes.length is expected to be 3'
);
