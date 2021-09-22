// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "setUTCDate" is 1
esid: sec-date.prototype.setutcdate
description: The "length" property of the "setUTCDate" is 1
---*/
assert.sameValue(
  Date.prototype.setUTCDate.hasOwnProperty("length"),
  true,
  'Date.prototype.setUTCDate.hasOwnProperty("length") must return true'
);

assert.sameValue(
  Date.prototype.setUTCDate.length,
  1,
  'The value of Date.prototype.setUTCDate.length is expected to be 1'
);
