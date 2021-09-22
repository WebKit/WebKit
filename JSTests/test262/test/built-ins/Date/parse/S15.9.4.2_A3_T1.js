// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date.parse property "length" has { ReadOnly, DontDelete, DontEnum }
    attributes
esid: sec-date.parse
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.parse.length;
verifyNotWritable(Date.parse, "length", null, 1);
assert.sameValue(Date.parse.length, x, 'The value of Date.parse.length is expected to equal the value of x');

// TODO: Convert to verifyProperty() format.
