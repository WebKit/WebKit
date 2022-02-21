// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "setFullYear" is 3
esid: sec-date.prototype.setfullyear
description: The "length" property of the "setFullYear" is 3
---*/
assert.sameValue(
  Date.prototype.setFullYear.hasOwnProperty("length"),
  true,
  'Date.prototype.setFullYear.hasOwnProperty("length") must return true'
);

assert.sameValue(
  Date.prototype.setFullYear.length,
  3,
  'The value of Date.prototype.setFullYear.length is expected to be 3'
);
