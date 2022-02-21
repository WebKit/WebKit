// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "setHours" is 4
esid: sec-date.prototype.sethours
description: The "length" property of the "setHours" is 4
---*/
assert.sameValue(
  Date.prototype.setHours.hasOwnProperty("length"),
  true,
  'Date.prototype.setHours.hasOwnProperty("length") must return true'
);

assert.sameValue(Date.prototype.setHours.length, 4, 'The value of Date.prototype.setHours.length is expected to be 4');
