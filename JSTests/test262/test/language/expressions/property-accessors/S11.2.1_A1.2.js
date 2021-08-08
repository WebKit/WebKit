// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    White Space and Line Terminator between "[" and MemberExpression or
    CallExpression and between Identifier and "]" are allowed
es5id: 11.2.1_A1.2
description: Checking by using eval
---*/

//CHECK#1
if (eval('Number[\u0009"POSITIVE_INFINITY"\u0009]') !== Number.POSITIVE_INFINITY) {
  throw new Test262Error('#1: Number[\\u0009"POSITIVE_INFINITY"\\u0009] === Number.POSITIVE_INFINITY');
}

//CHECK#2
if (eval('Number[\u000B"POSITIVE_INFINITY"\u000B]') !== Number.POSITIVE_INFINITY) {
  throw new Test262Error('#2: Number[\\u000B"POSITIVE_INFINITY"\\u000B] === Number.POSITIVE_INFINITY');  
}

//CHECK#3
if (eval('Number[\u000C"POSITIVE_INFINITY"\u000C]') !== Number.POSITIVE_INFINITY) {
  throw new Test262Error('#3: Number[\\u000C"POSITIVE_INFINITY"\\u000C] === Number.POSITIVE_INFINITY');
}

//CHECK#4
if (eval('Number[\u0020"POSITIVE_INFINITY"\u0020]') !== Number.POSITIVE_INFINITY) {
  throw new Test262Error('#4: Number[\\u0020"POSITIVE_INFINITY"\\u0020] === Number.POSITIVE_INFINITY');
}

//CHECK#5
if (eval('Number[\u00A0"POSITIVE_INFINITY"\u00A0]') !== Number.POSITIVE_INFINITY) {
  throw new Test262Error('#5: Number[\\u00A0"POSITIVE_INFINITY"\\u00A0] === Number.POSITIVE_INFINITY');
}

//CHECK#6
if (eval('Number[\u000A"POSITIVE_INFINITY"\u000A]') !== Number.POSITIVE_INFINITY) {
  throw new Test262Error('#6: Number[\\u000A"POSITIVE_INFINITY"\\u000A] === Number.POSITIVE_INFINITY');  
}

//CHECK#7
if (eval('Number[\u000D"POSITIVE_INFINITY"\u000D]') !== Number.POSITIVE_INFINITY) {
  throw new Test262Error('#7: Number[\\u000D"POSITIVE_INFINITY"\\u000D] === Number.POSITIVE_INFINITY');
}

//CHECK#8
if (eval('Number[\u2028"POSITIVE_INFINITY"\u2028]') !== Number.POSITIVE_INFINITY) {
  throw new Test262Error('#8: Number[\\u2028"POSITIVE_INFINITY"\\u2028] === Number.POSITIVE_INFINITY');
}

//CHECK#9
if (eval('Number[\u2029"POSITIVE_INFINITY"\u2029]') !== Number.POSITIVE_INFINITY) {
  throw new Test262Error('#9: Number[\\u2029"POSITIVE_INFINITY"\\u2029] === Number.POSITIVE_INFINITY');
}

//CHECK#10
if (eval('Number[\u0009\u000B\u000C\u0020\u00A0\u000A\u000D\u2028\u2029"POSITIVE_INFINITY"\u0009\u000B\u000C\u0020\u00A0\u000A\u000D\u2028\u2029]') !== Number.POSITIVE_INFINITY) {
  throw new Test262Error('#10: Number[\\u0009\\u000B\\u000C\\u0020\\u00A0\\u000A\\u000D\\u2028\\u2029"POSITIVE_INFINITY"\\u0009\\u000B\\u000C\\u0020\\u00A0\\u000A\\u000D\\u2028\\u2029] === Number.POSITIVE_INFINITY');
}
