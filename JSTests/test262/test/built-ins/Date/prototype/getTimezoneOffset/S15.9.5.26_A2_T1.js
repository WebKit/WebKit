// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "getTimezoneOffset" is 0
esid: sec-date.prototype.gettimezoneoffset
description: The "length" property of the "getTimezoneOffset" is 0
---*/
assert.sameValue(
  Date.prototype.getTimezoneOffset.hasOwnProperty("length"),
  true,
  'Date.prototype.getTimezoneOffset.hasOwnProperty("length") must return true'
);

assert.sameValue(
  Date.prototype.getTimezoneOffset.length,
  0,
  'The value of Date.prototype.getTimezoneOffset.length is expected to be 0'
);
