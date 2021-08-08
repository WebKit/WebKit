// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    If Type(Primitive(x)) is not String and Type(Primitive(y)) is not String,
    then operator x + y returns ToNumber(x) + ToNumber(y)
es5id: 11.6.1_A3.1_T2.3
description: >
    Type(Primitive(x)) is different from Type(Primitive(y)) and both
    types vary between Number (primitive or object) and Undefined
---*/

//CHECK#1
if (isNaN(1 + undefined) !== true) {
  throw new Test262Error('#1: 1 + undefined === Not-a-Number. Actual: ' + (1 + undefined));
}

//CHECK#2
if (isNaN(undefined + 1) !== true) {
  throw new Test262Error('#2: undefined + 1 === Not-a-Number. Actual: ' + (undefined + 1));
}

//CHECK#3
if (isNaN(new Number(1) + undefined) !== true) {
  throw new Test262Error('#3: new Number(1) + undefined === Not-a-Number. Actual: ' + (new Number(1) + undefined));
}

//CHECK#4
if (isNaN(undefined + new Number(1)) !== true) {
  throw new Test262Error('#4: undefined + new Number(1) === Not-a-Number. Actual: ' + (undefined + new Number(1)));
}
