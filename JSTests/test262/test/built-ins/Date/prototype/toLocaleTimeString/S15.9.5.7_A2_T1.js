// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "toLocaleTimeString" is 0
esid: sec-date.prototype.tolocaletimestring
description: The "length" property of the "toLocaleTimeString" is 0
---*/

assert.sameValue(
  Date.prototype.toLocaleTimeString.hasOwnProperty("length"),
  true,
  'Date.prototype.toLocaleTimeString.hasOwnProperty("length") must return true'
);

assert.sameValue(
  Date.prototype.toLocaleTimeString.length,
  0,
  'The value of Date.prototype.toLocaleTimeString.length is expected to be 0'
);
