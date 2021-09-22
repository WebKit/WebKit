// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.setUTCSeconds property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.setutcseconds
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.setUTCSeconds.length;
verifyNotWritable(Date.prototype.setUTCSeconds, "length", null, 1);

assert.sameValue(
  Date.prototype.setUTCSeconds.length,
  x,
  'The value of Date.prototype.setUTCSeconds.length is expected to equal the value of x'
);

// TODO: Convert to verifyProperty() format.
