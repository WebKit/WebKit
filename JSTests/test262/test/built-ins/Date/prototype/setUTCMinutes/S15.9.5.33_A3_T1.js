// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setUTCMinutes property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setutcminutes
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.setUTCMinutes.length;
verifyNotWritable(Date.prototype.setUTCMinutes, "length", null, 1);

assert.sameValue(
  Date.prototype.setUTCMinutes.length,
  x,
  'The value of Date.prototype.setUTCMinutes.length is expected to equal the value of x'
);

// TODO: Convert to verifyProperty() format.
