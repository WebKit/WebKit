// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "getDate" is 0
esid: sec-date.prototype.getdate
description: The "length" property of the "getDate" is 0
---*/
assert.sameValue(
  Date.prototype.getDate.hasOwnProperty("length"),
  true,
  'Date.prototype.getDate.hasOwnProperty("length") must return true'
);

assert.sameValue(Date.prototype.getDate.length, 0, 'The value of Date.prototype.getDate.length is expected to be 0');
