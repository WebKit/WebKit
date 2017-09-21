// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Operator use ToString
es5id: 15.1.2.2_A1_T3
es6id: 18.2.5
esid: sec-parseint-string-radix
description: Checking for undefined and null
---*/

assert.sameValue(parseInt(undefined), NaN, "undefined");
assert.sameValue(parseInt(null), NaN, "null");
