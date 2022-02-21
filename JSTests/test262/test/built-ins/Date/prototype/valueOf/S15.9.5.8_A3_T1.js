// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.valueOf property "length" has { ReadOnly, DontDelete,
    DontEnum } attributes
esid: sec-date.prototype.valueof
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.valueOf.length;
verifyNotWritable(Date.prototype.valueOf, "length", null, 1);

assert.sameValue(
  Date.prototype.valueOf.length,
  x,
  'The value of Date.prototype.valueOf.length is expected to equal the value of x'
);

// TODO: Convert to verifyProperty() format.
