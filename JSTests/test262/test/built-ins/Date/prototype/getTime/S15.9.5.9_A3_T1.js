// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.getTime property "length" has { ReadOnly, DontDelete,
    DontEnum } attributes
esid: sec-date.prototype.getseconds
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.getTime.length;
verifyNotWritable(Date.prototype.getTime, "length", null, 1);

assert.sameValue(
  Date.prototype.getTime.length,
  x,
  'The value of Date.prototype.getTime.length is expected to equal the value of x'
);

// TODO: Convert to verifyProperty() format.
