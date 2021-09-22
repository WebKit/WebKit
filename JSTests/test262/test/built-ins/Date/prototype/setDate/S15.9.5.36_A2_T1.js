// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "setDate" is 1
esid: sec-date.prototype.setdate
description: The "length" property of the "setDate" is 1
---*/
assert.sameValue(
  Date.prototype.setDate.hasOwnProperty("length"),
  true,
  'Date.prototype.setDate.hasOwnProperty("length") must return true'
);

assert.sameValue(Date.prototype.setDate.length, 1, 'The value of Date.prototype.setDate.length is expected to be 1');
