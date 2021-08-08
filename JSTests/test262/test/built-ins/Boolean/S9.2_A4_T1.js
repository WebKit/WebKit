// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    Result of boolean conversion from number value is false if the argument
    is +0, -0, or NaN; otherwise, is true
esid: sec-toboolean
description: +0, -0 and NaN convert to Boolean by explicit transformation
---*/

// CHECK#1
if (Boolean(+0) !== false) {
  throw new Test262Error('#1: Boolean(+0) === false. Actual: ' + (Boolean(+0)));
}

// CHECK#2
if (Boolean(-0) !== false) {
  throw new Test262Error('#2: Boolean(-0) === false. Actual: ' + (Boolean(-0)));
}

// CHECK#3
if (Boolean(Number.NaN) !== false) {
  throw new Test262Error('#3: Boolean(Number.NaN) === false. Actual: ' + (Boolean(Number.NaN)));
}
