// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "toDateString" is 0
esid: sec-date.prototype.todatestring
description: The "length" property of the "toDateString" is 0
---*/
assert.sameValue(
  Date.prototype.toDateString.hasOwnProperty("length"),
  true,
  'Date.prototype.toDateString.hasOwnProperty("length") must return true'
);

assert.sameValue(
  Date.prototype.toDateString.length,
  0,
  'The value of Date.prototype.toDateString.length is expected to be 0'
);
