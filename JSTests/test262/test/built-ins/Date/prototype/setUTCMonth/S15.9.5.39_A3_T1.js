// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setUTCMonth property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setutcmonth
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.setUTCMonth.length;
verifyNotWritable(Date.prototype.setUTCMonth, "length", null, 1);

assert.sameValue(
  Date.prototype.setUTCMonth.length,
  x,
  'The value of Date.prototype.setUTCMonth.length is expected to equal the value of x'
);

// TODO: Convert to verifyProperty() format.
