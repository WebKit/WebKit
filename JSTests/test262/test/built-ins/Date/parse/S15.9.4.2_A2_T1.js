// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The "length" property of the "parse" is 1
esid: sec-date.parse
description: The "length" property of the "parse" is 1
---*/

if (Date.parse.hasOwnProperty("length") !== true) {
  throw new Test262Error('#1: The parse has a "length" property');
}

if (Date.parse.length !== 1) {
  throw new Test262Error('#2: The "length" property of the parse is 1');
}
