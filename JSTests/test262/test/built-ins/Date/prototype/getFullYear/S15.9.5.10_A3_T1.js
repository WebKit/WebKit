// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.getfullyear
info: |
    The Date.prototype.getFullYear property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
es5id: 15.9.5.10_A3_T1
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.getFullYear.length;
verifyNotWritable(Date.prototype.getFullYear, "length", null, 1);

assert.sameValue(
  Date.prototype.getFullYear.length,
  x,
  'The value of Date.prototype.getFullYear.length is expected to equal the value of x'
);

// TODO: Convert to verifyProperty() format.
