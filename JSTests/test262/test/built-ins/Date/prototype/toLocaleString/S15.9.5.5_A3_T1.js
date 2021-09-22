// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.toLocaleString property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.tolocalestring
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.toLocaleString.length;
verifyNotWritable(Date.prototype.toLocaleString, "length", null, 1);

assert.sameValue(
  Date.prototype.toLocaleString.length,
  x,
  'The value of Date.prototype.toLocaleString.length is expected to equal the value of x'
);

// TODO: Convert to verifyProperty() format.
