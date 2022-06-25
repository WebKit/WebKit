// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.getDate property "length" has { ReadOnly, DontDelete,
    DontEnum } attributes
esid: sec-date.prototype.getdate
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.getDate.length;
verifyNotWritable(Date.prototype.getDate, "length", null, 1);

assert.sameValue(
  Date.prototype.getDate.length,
  x,
  'The value of Date.prototype.getDate.length is expected to equal the value of x'
);

// TODO: Convert to verifyProperty() format.
