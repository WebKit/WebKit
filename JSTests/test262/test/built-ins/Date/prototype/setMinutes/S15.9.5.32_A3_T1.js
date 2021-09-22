// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setMinutes property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setminutes
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.setMinutes.length;
verifyNotWritable(Date.prototype.setMinutes, "length", null, 1);

assert.sameValue(
  Date.prototype.setMinutes.length,
  x,
  'The value of Date.prototype.setMinutes.length is expected to equal the value of x'
);

// TODO: Convert to verifyProperty() format.
