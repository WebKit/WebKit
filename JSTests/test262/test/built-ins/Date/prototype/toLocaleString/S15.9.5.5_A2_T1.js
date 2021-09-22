// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "toLocaleString" is 0
esid: sec-date.prototype.tolocalestring
description: The "length" property of the "toLocaleString" is 0
---*/

assert.sameValue(
  Date.prototype.toLocaleString.hasOwnProperty("length"),
  true,
  'Date.prototype.toLocaleString.hasOwnProperty("length") must return true'
);

assert.sameValue(
  Date.prototype.toLocaleString.length,
  0,
  'The value of Date.prototype.toLocaleString.length is expected to be 0'
);
