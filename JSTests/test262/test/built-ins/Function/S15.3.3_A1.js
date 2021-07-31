// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Function constructor has the property "prototype"
es5id: 15.3.3_A1
description: Checking existence of the property "prototype"
---*/

if (!Function.hasOwnProperty("prototype")) {
  throw new Test262Error('#1: The Function constructor has the property "prototype"');
}
