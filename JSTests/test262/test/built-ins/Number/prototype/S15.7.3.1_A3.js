// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Number.prototype value is +0
es5id: 15.7.3.1_A3
description: Checking value of Number.prototype property
---*/

//CHECK#1
if (Number.prototype != 0) {
  throw new Test262Error('#2: Number.prototype == +0');
} else if (1 / Number.prototype != Number.POSITIVE_INFINITY) {
  throw new Test262Error('#2: Number.prototype == +0');
}
