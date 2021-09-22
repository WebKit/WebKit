// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setTime property "length" has { ReadOnly, DontDelete,
    DontEnum } attributes
esid: sec-date.prototype.settime
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.setTime.length;
verifyNotWritable(Date.prototype.setTime, "length", null, 1);

assert.sameValue(
  Date.prototype.setTime.length,
  x,
  'The value of Date.prototype.setTime.length is expected to equal the value of x'
);

// TODO: Convert to verifyProperty() format.
