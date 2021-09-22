// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.getUTCMilliseconds property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.getutcmilliseconds
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.getUTCMilliseconds.length;
verifyNotWritable(Date.prototype.getUTCMilliseconds, "length", null, 1);

assert.sameValue(
  Date.prototype.getUTCMilliseconds.length,
  x,
  'The value of Date.prototype.getUTCMilliseconds.length is expected to equal the value of x'
);

// TODO: Convert to verifyProperty() format.
