// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.constructor property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.constructor
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.constructor.length;
verifyNotWritable(Date.prototype.constructor, "length", null, 1);

assert.sameValue(
  Date.prototype.constructor.length,
  x,
  'The value of Date.prototype.constructor.length is expected to equal the value of x'
);

// TODO: Convert to verifyProperty() format.
