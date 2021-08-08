// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Operator uses ToNumber
es5id: 9.7_A3.1_T1
description: Type(x) is Boolean
---*/

// CHECK#1
if (String.fromCharCode(new Boolean(true)).charCodeAt(0) !== 1) {
  throw new Test262Error('#1: String.fromCharCode(new Boolean(true)).charCodeAt(0) === 1. Actual: ' + (String.fromCharCode(new Boolean(true)).charCodeAt(0)));
}

// CHECK#2
if (String.fromCharCode(false).charCodeAt(0) !== 0) {
  throw new Test262Error('#2: String.fromCharCode(false).charCodeAt(0) === 0. Actual: ' + (String.fromCharCode(false).charCodeAt(0)));
}
