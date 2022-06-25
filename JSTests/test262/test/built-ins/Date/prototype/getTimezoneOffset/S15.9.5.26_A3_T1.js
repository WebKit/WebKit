// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.getTimezoneOffset property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.gettimezoneoffset
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.getTimezoneOffset.length;
verifyNotWritable(Date.prototype.getTimezoneOffset, "length", null, 1);

assert.sameValue(
  Date.prototype.getTimezoneOffset.length,
  x,
  'The value of Date.prototype.getTimezoneOffset.length is expected to equal the value of x'
);

// TODO: Convert to verifyProperty() format.
