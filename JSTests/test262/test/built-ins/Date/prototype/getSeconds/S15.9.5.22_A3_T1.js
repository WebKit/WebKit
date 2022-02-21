// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.getSeconds property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.getseconds
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.getSeconds.length;
verifyNotWritable(Date.prototype.getSeconds, "length", null, 1);

assert.sameValue(
  Date.prototype.getSeconds.length,
  x,
  'The value of Date.prototype.getSeconds.length is expected to equal the value of x'
);

// TODO: Convert to verifyProperty() format.
