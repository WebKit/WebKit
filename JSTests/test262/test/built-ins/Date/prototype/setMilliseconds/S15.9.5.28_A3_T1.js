// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setMilliseconds property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setmilliseconds
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.setMilliseconds.length;
verifyNotWritable(Date.prototype.setMilliseconds, "length", null, 1);

assert.sameValue(
  Date.prototype.setMilliseconds.length,
  x,
  'The value of Date.prototype.setMilliseconds.length is expected to equal the value of x'
);

// TODO: Convert to verifyProperty() format.
