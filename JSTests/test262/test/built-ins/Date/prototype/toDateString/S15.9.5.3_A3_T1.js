// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.toDateString property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.todatestring
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.toDateString.length;
verifyNotWritable(Date.prototype.toDateString, "length", null, 1);

assert.sameValue(
  Date.prototype.toDateString.length,
  x,
  'The value of Date.prototype.toDateString.length is expected to equal the value of x'
);

// TODO: Convert to verifyProperty() format.
