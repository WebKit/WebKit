// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.utc
info: The "length" property of the "UTC" is 7
es5id: 15.9.4.3_A2_T1
description: The "length" property of the "UTC" is 7
---*/
assert.sameValue(Date.UTC.hasOwnProperty("length"), true, 'Date.UTC.hasOwnProperty("length") must return true');
assert.sameValue(Date.UTC.length, 7, 'The value of Date.UTC.length is expected to be 7');
