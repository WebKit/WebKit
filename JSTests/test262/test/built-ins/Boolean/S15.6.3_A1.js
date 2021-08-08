// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Boolean constructor has the property "prototype"
esid: sec-boolean.prototype
description: Checking existence of the property "prototype"
---*/

if (!Boolean.hasOwnProperty("prototype")) {
  throw new Test262Error('#1: The Boolean constructor has the property "prototype"');
}
