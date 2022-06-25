// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "toString" is 0
esid: sec-date.prototype.tostring
description: The "length" property of the "toString" is 0
---*/

assert.sameValue(
  Date.prototype.toString.hasOwnProperty("length"),
  true,
  'Date.prototype.toString.hasOwnProperty("length") must return true'
);

assert.sameValue(Date.prototype.toString.length, 0, 'The value of Date.prototype.toString.length is expected to be 0');
