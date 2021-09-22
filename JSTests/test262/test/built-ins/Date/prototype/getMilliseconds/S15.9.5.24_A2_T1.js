// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.getmilliseconds
info: The "length" property of the "getMilliseconds" is 0
es5id: 15.9.5.24_A2_T1
description: The "length" property of the "getMilliseconds" is 0
---*/
assert.sameValue(
  Date.prototype.getMilliseconds.hasOwnProperty("length"),
  true,
  'Date.prototype.getMilliseconds.hasOwnProperty("length") must return true'
);

assert.sameValue(
  Date.prototype.getMilliseconds.length,
  0,
  'The value of Date.prototype.getMilliseconds.length is expected to be 0'
);
