// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "setMilliseconds" is 1
esid: sec-date.prototype.setmilliseconds
description: The "length" property of the "setMilliseconds" is 1
---*/
assert.sameValue(
  Date.prototype.setMilliseconds.hasOwnProperty("length"),
  true,
  'Date.prototype.setMilliseconds.hasOwnProperty("length") must return true'
);

assert.sameValue(
  Date.prototype.setMilliseconds.length,
  1,
  'The value of Date.prototype.setMilliseconds.length is expected to be 1'
);
