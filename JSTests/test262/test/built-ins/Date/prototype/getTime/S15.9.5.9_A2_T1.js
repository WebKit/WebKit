// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "getTime" is 0
esid: sec-date.prototype.getseconds
description: The "length" property of the "getTime" is 0
---*/
assert.sameValue(
  Date.prototype.getTime.hasOwnProperty("length"),
  true,
  'Date.prototype.getTime.hasOwnProperty("length") must return true'
);

assert.sameValue(Date.prototype.getTime.length, 0, 'The value of Date.prototype.getTime.length is expected to be 0');
