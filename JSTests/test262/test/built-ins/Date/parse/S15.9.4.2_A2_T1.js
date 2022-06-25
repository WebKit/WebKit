// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "parse" is 1
esid: sec-date.parse
description: The "length" property of the "parse" is 1
---*/
assert.sameValue(Date.parse.hasOwnProperty("length"), true, 'Date.parse.hasOwnProperty("length") must return true');
assert.sameValue(Date.parse.length, 1, 'The value of Date.parse.length is expected to be 1');
