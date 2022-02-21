// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.prototype.getminutes
info: |
    The Date.prototype.getMinutes property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
es5id: 15.9.5.20_A3_T1
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.getMinutes.length;
verifyNotWritable(Date.prototype.getMinutes, "length", null, 1);

assert.sameValue(
  Date.prototype.getMinutes.length,
  x,
  'The value of Date.prototype.getMinutes.length is expected to equal the value of x'
);

// TODO: Convert to verifyProperty() format.
