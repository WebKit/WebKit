// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    Returns a boolean value (not a Boolean object) computed by
    ToBoolean(value)
esid: sec-terms-and-definitions-boolean-value
description: Used various assigning values to any variable as argument
---*/

var x;

//CHECK#1
if (typeof Boolean(x = 0) !== "boolean") {
  throw new Test262Error('#1.1: typeof Boolean(x=0) should be "boolean", actual is "' + typeof Boolean(x = 0) + '"');
}
if (Boolean(x = 0) !== false) {
  throw new Test262Error('#1.2: Boolean(x=0) should be false');
}

//CHECK#2
if (typeof Boolean(x = 1) !== "boolean") {
  throw new Test262Error('#2.1: typeof Boolean(x=1) should be "boolean", actual is "' + typeof Boolean(x = 1) + '"');
}
if (Boolean(x = 1) !== true) {
  throw new Test262Error('#2.2: Boolean(x=1) should be true');
}

//CHECK#3
if (typeof Boolean(x = false) !== "boolean") {
  throw new Test262Error('#3.1: typeof Boolean(x=false) should be "boolean", actual is "' + typeof Boolean(x = false) + '"');
}
if (Boolean(x = false) !== false) {
  throw new Test262Error('#3.2: Boolean(x=false) should be false');
}

//CHECK#4
if (typeof Boolean(x = true) !== "boolean") {
  throw new Test262Error('#4.1: typeof Boolean(x=true) should be "boolean", actual is "' + typeof Boolean(x = true) + '"');
}
if (Boolean(x = true) !== true) {
  throw new Test262Error('#4.2: Boolean(x=true) should be true');
}

//CHECK#5
if (typeof Boolean(x = null) !== "boolean") {
  throw new Test262Error('#5.1: typeof Boolean(x=null) should be "boolean", actual is "' + typeof Boolean(x = null) + '"');
}
if (Boolean(x = null) !== false) {
  throw new Test262Error('#5.2: Boolean(x=null) should be false');
}
