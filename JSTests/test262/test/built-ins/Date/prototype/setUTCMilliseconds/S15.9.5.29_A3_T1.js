// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setUTCMilliseconds property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setutcmilliseconds
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.setUTCMilliseconds.length;
verifyNotWritable(Date.prototype.setUTCMilliseconds, "length", null, 1);

assert.sameValue(
  Date.prototype.setUTCMilliseconds.length,
  x,
  'The value of Date.prototype.setUTCMilliseconds.length is expected to equal the value of x'
);

// TODO: Convert to verifyProperty() format.
