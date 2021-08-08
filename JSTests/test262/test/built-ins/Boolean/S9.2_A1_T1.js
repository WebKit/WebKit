// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Result of boolean conversion from undefined value is false
esid: sec-toboolean
description: >
    Undefined, void and others are converted to Boolean by explicit
    transformation
---*/

// CHECK#1
if (Boolean(undefined) !== false) {
  throw new Test262Error('#1: Boolean(undefined) === false. Actual: ' + (Boolean(undefined)));
}

// CHECK#2
if (Boolean(void 0) !== false) {
  throw new Test262Error('#2: Boolean(undefined) === false. Actual: ' + (Boolean(undefined)));
}

// CHECK#3
if (Boolean(eval("var x")) !== false) {
  throw new Test262Error('#3: Boolean(eval("var x")) === false. Actual: ' + (Boolean(eval("var x"))));
}

// CHECK#4
if (Boolean() !== false) {
  throw new Test262Error('#4: Boolean() === false. Actual: ' + (Boolean()));
}
