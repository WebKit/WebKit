// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.toString property "length" has { ReadOnly, DontDelete,
    DontEnum } attributes
esid: sec-date.prototype.tostring
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.toString.length;
verifyNotWritable(Date.prototype.toString, "length", null, 1);

assert.sameValue(
  Date.prototype.toString.length,
  x,
  'The value of Date.prototype.toString.length is expected to equal the value of x'
);

// TODO: Convert to verifyProperty() format.
