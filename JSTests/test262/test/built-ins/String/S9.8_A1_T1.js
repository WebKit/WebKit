// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Result of ToString conversion from undefined value is "undefined"
es5id: 9.8_A1_T1
description: >
    Undefined values is undefined, void 0 and eval("var x"). Use
    explicit transformation
---*/

// CHECK#1
if (String(undefined) !== "undefined") {
  throw new Test262Error('#1: String(undefined) === "undefined". Actual: ' + (String(undefined)));
}

// CHECK#2
if (String(void 0) !== "undefined") {
  throw new Test262Error('#2: String(void 0) === "undefined". Actual: ' + (String(void 0)));
}

// CHECK#3
if (String(eval("var x")) !== "undefined") {
  throw new Test262Error('#3: String(eval("var x")) === "undefined" . Actual: ' + (String(eval("var x"))));
}
