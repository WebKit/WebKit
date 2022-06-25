// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "getUTCMilliseconds" is 0
esid: sec-date.prototype.getutcmilliseconds
description: The "length" property of the "getUTCMilliseconds" is 0
---*/
assert.sameValue(
  Date.prototype.getUTCMilliseconds.hasOwnProperty("length"),
  true,
  'Date.prototype.getUTCMilliseconds.hasOwnProperty("length") must return true'
);

assert.sameValue(
  Date.prototype.getUTCMilliseconds.length,
  0,
  'The value of Date.prototype.getUTCMilliseconds.length is expected to be 0'
);
