// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Number.prototype.valueOf() returns this number value
es5id: 15.7.4.4_A1_T02
description: calling with argument
---*/

//CHECK#1
if (Number.prototype.valueOf("argument") !== 0) {
  throw new Test262Error('#1: Number.prototype.valueOf("argument") === 0');
}

//CHECK#2
if ((new Number()).valueOf("argument") !== 0) {
  throw new Test262Error('#2: (new Number()).valueOf("argument") === 0');
}

//CHECK#3
if ((new Number(0)).valueOf("argument") !== 0) {
  throw new Test262Error('#3: (new Number(0)).valueOf("argument") === 0');
}

//CHECK#4
if ((new Number(-1)).valueOf("argument") !== -1) {
  throw new Test262Error('#4: (new Number(-1)).valueOf("argument") === -1');
}

//CHECK#5
if ((new Number(1)).valueOf("argument") !== 1) {
  throw new Test262Error('#5: (new Number(1)).valueOf("argument") === 1');
}

//CHECK#6
assert.sameValue(
  new Number(NaN).valueOf("argument"),
  NaN,
  "NaN"
);

//CHECK#7
if ((new Number(Number.POSITIVE_INFINITY)).valueOf("argument") !== Number.POSITIVE_INFINITY) {
  throw new Test262Error('#7: (new Number(Number.POSITIVE_INFINITY)).valueOf("argument") === Infinity');
}

//CHECK#8
if ((new Number(Number.NEGATIVE_INFINITY)).valueOf("argument") !== Number.NEGATIVE_INFINITY) {
  throw new Test262Error('#8: (new Number(Number.NEGATIVE_INFINITY)).valueOf("argument") === -Infinity');
}
