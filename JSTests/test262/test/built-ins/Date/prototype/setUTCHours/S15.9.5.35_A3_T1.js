// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setUTCHours property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setutchours
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.setUTCHours.length;
verifyNotWritable(Date.prototype.setUTCHours, "length", null, 1);

assert.sameValue(
  Date.prototype.setUTCHours.length,
  x,
  'The value of Date.prototype.setUTCHours.length is expected to equal the value of x'
);

// TODO: Convert to verifyProperty() format.
