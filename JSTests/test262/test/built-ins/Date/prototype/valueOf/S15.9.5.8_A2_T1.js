// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "valueOf" is 0
esid: sec-date.prototype.valueof
description: The "length" property of the "valueOf" is 0
---*/

assert.sameValue(
  Date.prototype.valueOf.hasOwnProperty("length"),
  true,
  'Date.prototype.valueOf.hasOwnProperty("length") must return true'
);

assert.sameValue(Date.prototype.valueOf.length, 0, 'The value of Date.prototype.valueOf.length is expected to be 0');
