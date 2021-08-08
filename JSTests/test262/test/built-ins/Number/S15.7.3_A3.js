// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Number constructor has the property "MIN_VALUE"
es5id: 15.7.3_A3
description: Checking existence of the property "MIN_VALUE"
---*/

if (!Number.hasOwnProperty("MIN_VALUE")) {
  throw new Test262Error('#1: The Number constructor has the property "MIN_VALUE"');
}
