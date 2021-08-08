// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Operator uses GetValue
es5id: 11.13.2_A2.1_T3.6
description: >
    If GetBase(LeftHandSideExpression) is null, throw ReferenceError.
    Check operator is "x <<= y"
---*/

//CHECK#1
try {
  var z = (x <<= 1);
  throw new Test262Error('#1.1: x <<= 1 throw ReferenceError. Actual: ' + (z));  
}
catch (e) {
  if ((e instanceof ReferenceError) !== true) {
    throw new Test262Error('#1.2: x <<= 1 throw ReferenceError. Actual: ' + (e));  
  }
}
