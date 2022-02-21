// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "setSeconds" is 2
esid: sec-date.prototype.setseconds
description: The "length" property of the "setSeconds" is 2
---*/
assert.sameValue(
  Date.prototype.setSeconds.hasOwnProperty("length"),
  true,
  'Date.prototype.setSeconds.hasOwnProperty("length") must return true'
);

assert.sameValue(
  Date.prototype.setSeconds.length,
  2,
  'The value of Date.prototype.setSeconds.length is expected to be 2'
);
