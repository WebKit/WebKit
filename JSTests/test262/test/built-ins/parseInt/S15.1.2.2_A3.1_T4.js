// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Operator use ToNumber
es5id: 15.1.2.2_A3.1_T4
es6id: 18.2.5
esid: sec-parseint-string-radix
description: Checking for Boolean object
---*/

//CHECK#1
if (parseInt("11", new Boolean(false)) !== parseInt("11", false)) {
  $ERROR('#1: parseInt("11", new Boolean(false)) === parseInt("11", false). Actual: ' + (parseInt("11", new Boolean(false))));
}

//CHECK#2
assert.sameValue(parseInt("11", new Boolean(true)), NaN);
