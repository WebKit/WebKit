// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "setUTCHours" is 4
esid: sec-date.prototype.setutchours
description: The "length" property of the "setUTCHours" is 4
---*/
assert.sameValue(
  Date.prototype.setUTCHours.hasOwnProperty("length"),
  true,
  'Date.prototype.setUTCHours.hasOwnProperty("length") must return true'
);

assert.sameValue(
  Date.prototype.setUTCHours.length,
  4,
  'The value of Date.prototype.setUTCHours.length is expected to be 4'
);
