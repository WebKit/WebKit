// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    The Date property "prototype" has { DontEnum, DontDelete, ReadOnly }
    attributes
esid: sec-date.prototype
description: Checking ReadOnly attribute
includes: [propertyHelper.js]
---*/

var x = Date.prototype;
verifyNotWritable(Date, "prototype", null, 1);
assert.sameValue(Date.prototype, x, 'The value of Date.prototype is expected to equal the value of x');

// TODO: Convert to verifyProperty() format.
