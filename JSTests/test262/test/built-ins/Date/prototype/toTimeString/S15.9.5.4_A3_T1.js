// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.toTimeString property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.totimestring
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.toTimeString.length;
verifyNotWritable(Date.prototype.toTimeString, "length", null, 1);

assert.sameValue(
  Date.prototype.toTimeString.length,
  x,
  'The value of Date.prototype.toTimeString.length is expected to equal the value of x'
);

// TODO: Convert to verifyProperty() format.
