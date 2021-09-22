// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setMonth property "length" has { ReadOnly, DontDelete,
    DontEnum } attributes
esid: sec-date.prototype.setmonth
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.setMonth.length;
verifyNotWritable(Date.prototype.setMonth, "length", null, 1);

assert.sameValue(
  Date.prototype.setMonth.length,
  x,
  'The value of Date.prototype.setMonth.length is expected to equal the value of x'
);

// TODO: Convert to verifyProperty() format.
