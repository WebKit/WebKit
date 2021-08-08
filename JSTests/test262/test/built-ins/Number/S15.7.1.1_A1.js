// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    Number([value]) returns a number value (not a Number object) computed by
    ToNumber(value) if value was supplied
es5id: 15.7.1.1_A1
description: Used values "10", 10, new String("10"), new Object(10) and "abc"
---*/

//CHECK#1
if (typeof Number("10") !== "number") {
  throw new Test262Error('#1: typeof Number("10") should be "number", actual is "' + typeof Number("10") + '"');
}

//CHECK#2
if (typeof Number(10) !== "number") {
  throw new Test262Error('#2: typeof Number(10) should be "number", actual is "' + typeof Number(10) + '"');
}

//CHECK#3
if (typeof Number(new String("10")) !== "number") {
  throw new Test262Error('#3: typeof Number(new String("10")) should be "number", actual is "' + typeof Number(new String("10")) + '"');
}

//CHECK#4
if (typeof Number(new Object(10)) !== "number") {
  throw new Test262Error('#4: typeof Number(new Object(10)) should be "number", actual is "' + typeof Number(new Object(10)) + '"');
}

//CHECK #5
assert.sameValue(Number("abc"), NaN, "Number('abc')");
