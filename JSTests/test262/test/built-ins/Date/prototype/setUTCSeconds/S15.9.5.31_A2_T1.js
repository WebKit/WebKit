// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "setUTCSeconds" is 2
esid: sec-date.prototype.setutcseconds
description: The "length" property of the "setUTCSeconds" is 2
---*/
assert.sameValue(
  Date.prototype.setUTCSeconds.hasOwnProperty("length"),
  true,
  'Date.prototype.setUTCSeconds.hasOwnProperty("length") must return true'
);

assert.sameValue(
  Date.prototype.setUTCSeconds.length,
  2,
  'The value of Date.prototype.setUTCSeconds.length is expected to be 2'
);
