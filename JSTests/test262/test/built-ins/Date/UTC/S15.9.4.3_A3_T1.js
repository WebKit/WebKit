// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.utc
info: |
    The Date.UTC property "length" has { ReadOnly, DontDelete, DontEnum }
    attributes
es5id: 15.9.4.3_A3_T1
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.UTC.length;
verifyNotWritable(Date.UTC, "length", null, 1);
assert.sameValue(Date.UTC.length, x, 'The value of Date.UTC.length is expected to equal the value of x');

// TODO: Convert to verifyProperty() format.
