// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setHours property "length" has { ReadOnly, DontDelete,
    DontEnum } attributes
esid: sec-date.prototype.sethours
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.setHours.length;
verifyNotWritable(Date.prototype.setHours, "length", null, 1);

assert.sameValue(
  Date.prototype.setHours.length,
  x,
  'The value of Date.prototype.setHours.length is expected to equal the value of x'
);

// TODO: Convert to verifyProperty() format.
