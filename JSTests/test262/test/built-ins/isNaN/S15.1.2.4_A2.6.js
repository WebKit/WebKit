// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The isNaN property has not prototype property
esid: sec-isnan-number
description: Checking isNaN.prototype
---*/

//CHECK#1
if (isNaN.prototype !== undefined) {
  throw new Test262Error('#1: isNaN.prototype === undefined. Actual: ' + (isNaN.prototype));
}
