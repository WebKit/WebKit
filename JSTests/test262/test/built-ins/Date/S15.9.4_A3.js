// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date constructor has the property "UTC"
esid: sec-date-constructor
description: Checking existence of the property "UTC"
---*/

if (!Date.hasOwnProperty("UTC")) {
  throw new Test262Error('#1: The Date constructor has the property "UTC"');
}
