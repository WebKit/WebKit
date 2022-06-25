// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.getmilliseconds
info: |
    The Date.prototype.getMilliseconds property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
es5id: 15.9.5.24_A3_T1
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.getMilliseconds.length;
verifyNotWritable(Date.prototype.getMilliseconds, "length", null, 1);

assert.sameValue(
  Date.prototype.getMilliseconds.length,
  x,
  'The value of Date.prototype.getMilliseconds.length is expected to equal the value of x'
);

// TODO: Convert to verifyProperty() format.
