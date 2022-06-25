// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.getUTCFullYear property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.getutcfullyear
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.getUTCFullYear.length;
verifyNotWritable(Date.prototype.getUTCFullYear, "length", null, 1);

assert.sameValue(
  Date.prototype.getUTCFullYear.length,
  x,
  'The value of Date.prototype.getUTCFullYear.length is expected to equal the value of x'
);

// TODO: Convert to verifyProperty() format.
