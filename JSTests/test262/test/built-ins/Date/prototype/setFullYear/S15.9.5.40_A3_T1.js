// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setFullYear property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setfullyear
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.setFullYear.length;
verifyNotWritable(Date.prototype.setFullYear, "length", null, 1);

assert.sameValue(
  Date.prototype.setFullYear.length,
  x,
  'The value of Date.prototype.setFullYear.length is expected to equal the value of x'
);

// TODO: Convert to verifyProperty() format.
