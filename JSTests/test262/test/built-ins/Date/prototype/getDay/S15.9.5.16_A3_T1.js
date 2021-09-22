// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.getday
info: |
    The Date.prototype.getDay property "length" has { ReadOnly, DontDelete,
    DontEnum } attributes
es5id: 15.9.5.16_A3_T1
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.getDay.length;
verifyNotWritable(Date.prototype.getDay, "length", null, 1);

assert.sameValue(
  Date.prototype.getDay.length,
  x,
  'The value of Date.prototype.getDay.length is expected to equal the value of x'
);

// TODO: Convert to verifyProperty() format.
