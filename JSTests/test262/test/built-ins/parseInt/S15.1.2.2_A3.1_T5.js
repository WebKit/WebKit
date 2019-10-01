// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Operator use ToNumber
esid: sec-parseint-string-radix
description: Checking for Number object
---*/

//CHECK#1
if (parseInt("11", new Number(2)) !== parseInt("11", 2)) {
  $ERROR('#1: parseInt("11", new Number(2)) === parseInt("11", 2). Actual: ' + (parseInt("11", new Number(2))));
}

//CHECK#2
if (parseInt("11", new Number(Infinity)) !== parseInt("11", Infinity)) {
  $ERROR('#2: parseInt("11", new Number(Infinity)) === parseInt("11", Infinity). Actual: ' + (parseInt("11", new Number(Infinity))));
}
