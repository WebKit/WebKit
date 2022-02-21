// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.getUTCMinutes property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.getutcminutes
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.getUTCMinutes.length;
verifyNotWritable(Date.prototype.getUTCMinutes, "length", null, 1);

assert.sameValue(
  Date.prototype.getUTCMinutes.length,
  x,
  'The value of Date.prototype.getUTCMinutes.length is expected to equal the value of x'
);

// TODO: Convert to verifyProperty() format.
