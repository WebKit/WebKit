// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The length property of parseInt has the attribute ReadOnly
esid: sec-parseint-string-radix
description: Checking if varying the length property fails
includes: [propertyHelper.js]
---*/

assert.sameValue(parseInt.length, 2, 'The value of parseInt.length is 2');
verifyNotWritable(parseInt, "length", null, Infinity);
