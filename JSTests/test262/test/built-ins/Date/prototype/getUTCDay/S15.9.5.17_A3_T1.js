// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.getUTCDay property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.getutcdaty
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.getUTCDay.length;
verifyNotWritable(Date.prototype.getUTCDay, "length", null, 1);

assert.sameValue(
  Date.prototype.getUTCDay.length,
  x,
  'The value of Date.prototype.getUTCDay.length is expected to equal the value of x'
);

// TODO: Convert to verifyProperty() format.
