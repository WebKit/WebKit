// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Operator use ToString
es5id: 15.1.2.3_A1_T1
es6id: 18.2.4
esid: sec-parsefloat-string
description: Checking for boolean primitive
---*/

assert.sameValue(parseFloat(true), NaN, "true");
assert.sameValue(parseFloat(false), NaN, "false");
