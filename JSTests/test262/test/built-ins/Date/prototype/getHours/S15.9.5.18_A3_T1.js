// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.gethours
info: |
    The Date.prototype.getHours property "length" has { ReadOnly, DontDelete,
    DontEnum } attributes
es5id: 15.9.5.18_A3_T1
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.getHours.length;
verifyNotWritable(Date.prototype.getHours, "length", null, 1);

assert.sameValue(
  Date.prototype.getHours.length,
  x,
  'The value of Date.prototype.getHours.length is expected to equal the value of x'
);

// TODO: Convert to verifyProperty() format.
