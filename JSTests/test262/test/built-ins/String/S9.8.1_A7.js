// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    If 1 <= s < 1e21 or -1e21 s < -1 and s has a fractional
    component, return the string consisting of the most significant n digits of
    the decimal representation of s, followed by a decimal point '.',
    followed by the remaining k-n digits of the decimal representation of s
es5id: 9.8.1_A7
description: >
    1.0000001 and -1.0000001 convert to String by explicit
    transformation
---*/

// CHECK#1
if (String(1.0000001) !== "1.0000001") {
  throw new Test262Error('#1: String(1.0000001) === "1.0000001". Actual: ' + (String(1.0000001)));
}

// CHECK#2
if (String(-1.0000001) !== "-1.0000001") {
  throw new Test262Error('#2: String(-1.0000001) === "-1.0000001". Actual: ' + (String(-1.0000001)));
}
