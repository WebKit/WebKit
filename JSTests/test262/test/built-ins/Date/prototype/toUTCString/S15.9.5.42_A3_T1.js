// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.toUTCString property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.toutcstring
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.toUTCString.length;
verifyNotWritable(Date.prototype.toUTCString, "length", null, 1);

assert.sameValue(
  Date.prototype.toUTCString.length,
  x,
  'The value of Date.prototype.toUTCString.length is expected to equal the value of x'
);

// TODO: Convert to verifyProperty() format.
