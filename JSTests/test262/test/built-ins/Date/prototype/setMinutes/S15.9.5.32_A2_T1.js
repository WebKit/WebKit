// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "setMinutes" is 3
esid: sec-date.prototype.setminutes
description: The "length" property of the "setMinutes" is 3
---*/
assert.sameValue(
  Date.prototype.setMinutes.hasOwnProperty("length"),
  true,
  'Date.prototype.setMinutes.hasOwnProperty("length") must return true'
);

assert.sameValue(
  Date.prototype.setMinutes.length,
  3,
  'The value of Date.prototype.setMinutes.length is expected to be 3'
);
