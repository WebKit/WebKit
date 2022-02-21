// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
info: The "length" property of the "toTimeString" is 0
esid: sec-date.prototype.totimestring
description: The "length" property of the "toTimeString" is 0
---*/

assert.sameValue(
  Date.prototype.toTimeString.hasOwnProperty("length"),
  true,
  'Date.prototype.toTimeString.hasOwnProperty("length") must return true'
);


assert.sameValue(
  Date.prototype.toTimeString.length,
  0,
  'The value of Date.prototype.toTimeString.length is expected to be 0'
);
