// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Operator remove leading StrWhiteSpaceChar
esid: sec-parsefloat-string
description: "StrWhiteSpaceChar :: PS (U+2029)"
---*/

//CHECK#1
if (parseFloat("\u20291.1") !== parseFloat("1.1")) {
  throw new Test262Error('#1: parseFloat("\\u20291.1") === parseFloat("1.1"). Actual: ' + (parseFloat("\u20291.1")));
}

//CHECK#2
if (parseFloat("\u2029\u2029-1.1") !== parseFloat("-1.1")) {
  throw new Test262Error('#2: parseFloat("\\u2029\\u2029-1.1") === parseFloat("-1.1"). Actual: ' + (parseFloat("\u2029\u2029-1.1")));
}

//CHECK#3
assert.sameValue(parseFloat("\u2029"), NaN);
