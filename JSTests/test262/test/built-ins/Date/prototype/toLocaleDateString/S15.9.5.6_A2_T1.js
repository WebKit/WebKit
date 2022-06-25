// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "toLocaleDateString" is 0
esid: sec-date.prototype.tolocaledatestring
description: The "length" property of the "toLocaleDateString" is 0
---*/
assert.sameValue(
  Date.prototype.toLocaleDateString.hasOwnProperty("length"),
  true,
  'Date.prototype.toLocaleDateString.hasOwnProperty("length") must return true'
);

assert.sameValue(
  Date.prototype.toLocaleDateString.length,
  0,
  'The value of Date.prototype.toLocaleDateString.length is expected to be 0'
);
