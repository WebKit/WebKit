// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "toUTCString" is 0
esid: sec-date.prototype.toutcstring
description: The "length" property of the "toUTCString" is 0
---*/

assert.sameValue(
  Date.prototype.toUTCString.hasOwnProperty("length"),
  true,
  'Date.prototype.toUTCString.hasOwnProperty("length") must return true'
);

assert.sameValue(
  Date.prototype.toUTCString.length,
  0,
  'The value of Date.prototype.toUTCString.length is expected to be 0'
);
