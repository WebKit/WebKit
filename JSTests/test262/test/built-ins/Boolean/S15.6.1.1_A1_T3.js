// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    Returns a boolean value (not a Boolean object) computed by
    ToBoolean(value)
esid: sec-terms-and-definitions-boolean-value
description: Used various string values as argument
---*/

//CHECK#1
if (typeof Boolean("0") !== "boolean") {
  throw new Test262Error('#1.1: typeof Boolean("0") should be "boolean", actual is "' + typeof Boolean("0") + '"');
}
if (Boolean("0") !== true) {
  throw new Test262Error('#1.2: Boolean("0") should be true');
}

//CHECK#2
if (typeof Boolean("-1") !== "boolean") {
  throw new Test262Error('#2.1: typeof Boolean("-1") should be "boolean", actual is "' + typeof Boolean("-1") + '"');
}
if (Boolean("-1") !== true) {
  throw new Test262Error('#2.2: Boolean("-1") should be true');
}

//CHECK#3
if (typeof Boolean("1") !== "boolean") {
  throw new Test262Error('#3.1: typeof Boolean("1") should be "boolean", actual is "' + typeof Boolean("1") + '"');
}
if (Boolean("1") !== true) {
  throw new Test262Error('#3.2: Boolean("1") should be true');
}

//CHECK#4
if (typeof Boolean("false") !== "boolean") {
  throw new Test262Error('#4.1: typeof Boolean("false") should be "boolean", actual is "' + typeof Boolean("false") + '"');
}
if (Boolean("false") !== true) {
  throw new Test262Error('#4.2: Boolean("false") should be true');
}

//CHECK#5
if (typeof Boolean("true") !== "boolean") {
  throw new Test262Error('#5.1: typeof Boolean("true") should be "boolean", actual is "' + typeof Boolean("true") + '"');
}
if (Boolean("true") !== true) {
  throw new Test262Error('#5.2: Boolean("true") should be true');
}
