// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "setTime" is 1
esid: sec-date.prototype.settime
description: The "length" property of the "setTime" is 1
---*/
assert.sameValue(
  Date.prototype.setTime.hasOwnProperty("length"),
  true,
  'Date.prototype.setTime.hasOwnProperty("length") must return true'
);

assert.sameValue(Date.prototype.setTime.length, 1, 'The value of Date.prototype.setTime.length is expected to be 1');
