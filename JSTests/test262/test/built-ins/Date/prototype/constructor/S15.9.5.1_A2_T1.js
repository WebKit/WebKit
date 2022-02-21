// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "constructor" is 7
esid: sec-date.prototype.constructor
description: The "length" property of the "constructor" is 7
---*/
assert.sameValue(
  Date.prototype.constructor.hasOwnProperty("length"),
  true,
  'Date.prototype.constructor.hasOwnProperty("length") must return true'
);

assert.sameValue(
  Date.prototype.constructor.length,
  7,
  'The value of Date.prototype.constructor.length is expected to be 7'
);
