// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.prototype.toLocaleTimeString property "length" has { ReadOnly,
    DontDelete, DontEnum } attributes
esid: sec-date.prototype.tolocaletimestring
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype.toLocaleTimeString.length;
verifyNotWritable(Date.prototype.toLocaleTimeString, "length", null, 1);

assert.sameValue(
  Date.prototype.toLocaleTimeString.length,
  x,
  'The value of Date.prototype.toLocaleTimeString.length is expected to equal the value of x'
);

// TODO: Convert to verifyProperty() format.
