// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "setUTCMilliseconds" is 1
esid: sec-date.prototype.setutcmilliseconds
description: The "length" property of the "setUTCMilliseconds" is 1
---*/
assert.sameValue(
  Date.prototype.setUTCMilliseconds.hasOwnProperty("length"),
  true,
  'Date.prototype.setUTCMilliseconds.hasOwnProperty("length") must return true'
);

assert.sameValue(
  Date.prototype.setUTCMilliseconds.length,
  1,
  'The value of Date.prototype.setUTCMilliseconds.length is expected to be 1'
);
