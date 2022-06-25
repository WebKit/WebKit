// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.getUTCSeconds property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.getutcseconds
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.getUTCSeconds.length;
verifyNotWritable(Date.prototype.getUTCSeconds, "length", null, 1);

assert.sameValue(
  Date.prototype.getUTCSeconds.length,
  x,
  'The value of Date.prototype.getUTCSeconds.length is expected to equal the value of x'
);

// TODO: Convert to verifyProperty() format.
