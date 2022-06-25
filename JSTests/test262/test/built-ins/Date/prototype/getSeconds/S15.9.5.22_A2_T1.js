// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "getSeconds" is 0
esid: sec-date.prototype.getseconds
description: The "length" property of the "getSeconds" is 0
---*/
assert.sameValue(
  Date.prototype.getSeconds.hasOwnProperty("length"),
  true,
  'Date.prototype.getSeconds.hasOwnProperty("length") must return true'
);

assert.sameValue(
  Date.prototype.getSeconds.length,
  0,
  'The value of Date.prototype.getSeconds.length is expected to be 0'
);
