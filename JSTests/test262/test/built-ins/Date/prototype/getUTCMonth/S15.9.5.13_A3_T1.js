// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.getUTCMonth property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.getutcmonth
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.getUTCMonth.length;
verifyNotWritable(Date.prototype.getUTCMonth, "length", null, 1);

assert.sameValue(
  Date.prototype.getUTCMonth.length,
  x,
  'The value of Date.prototype.getUTCMonth.length is expected to equal the value of x'
);

// TODO: Convert to verifyProperty() format.
