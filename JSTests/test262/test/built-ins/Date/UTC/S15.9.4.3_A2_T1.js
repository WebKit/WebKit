// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-date.utc
info: The "length" property of the "UTC" is 7
es5id: 15.9.4.3_A2_T1
description: The "length" property of the "UTC" is 7
---*/

if (Date.UTC.hasOwnProperty("length") !== true) {
  throw new Test262Error('#1: The UTC has a "length" property');
}

if (Date.UTC.length !== 7) {
  throw new Test262Error('#2: The "length" property of the UTC is 7');
}
