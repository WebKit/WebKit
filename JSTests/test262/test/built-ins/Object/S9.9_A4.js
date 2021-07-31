// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: |
    ToObject conversion from Number: create a new Number object
    whose [[value]] property is set to the value of the number
es5id: 9.9_A4
description: Converting from various numbers to Object
---*/

// CHECK#1
if (Object(0).valueOf() !== 0) {
  throw new Test262Error('#1: Object(0).valueOf() === 0. Actual: ' + (Object(0).valueOf()));
}

// CHECK#2
if (typeof Object(0) !== "object") {
  throw new Test262Error('#2: typeof Object(0) === "object". Actual: ' + (typeof Object(0)));
}

// CHECK#3
if (Object(0).constructor.prototype !== Number.prototype) {
  throw new Test262Error('#3: Object(0).constructor.prototype === Number.prototype. Actual: ' + (Object(0).constructor.prototype));
}

// CHECK#4
if (Object(-0).valueOf() !== -0) {
  throw new Test262Error('#4.1: Object(-0).valueOf() === 0. Actual: ' + (Object(-0).valueOf()));
} else if (1 / Object(-0).valueOf() !== Number.NEGATIVE_INFINITY) {
  throw new Test262Error('#4.2: Object(-0).valueOf() === -0. Actual: +0');
}

// CHECK#5
if (typeof Object(-0) !== "object") {
  throw new Test262Error('#5: typeof Object(-0) === "object". Actual: ' + (typeof Object(-0)));
}

// CHECK#6
if (Object(-0).constructor.prototype !== Number.prototype) {
  throw new Test262Error('#6: Object(-0).constructor.prototype === Number.prototype. Actual: ' + (Object(-0).constructor.prototype));
}

// CHECK#7
if (Object(1).valueOf() !== 1) {
  throw new Test262Error('#7: Object(1).valueOf() === 1. Actual: ' + (Object(1).valueOf()));
}

// CHECK#8
if (typeof Object(1) !== "object") {
  throw new Test262Error('#8: typeof Object(1) === "object". Actual: ' + (typeof Object(1)));
}

// CHECK#9
if (Object(1).constructor.prototype !== Number.prototype) {
  throw new Test262Error('#9: Object(1).constructor.prototype === Number.prototype. Actual: ' + (Object(1).constructor.prototype));
}

// CHECK#10
if (Object(-1).valueOf() !== -1) {
  throw new Test262Error('#10: Object(-1).valueOf() === -1. Actual: ' + (Object(-1).valueOf()));
}

// CHECK#11
if (typeof Object(-1) !== "object") {
  throw new Test262Error('#11: typeof Object(-1) === "object". Actual: ' + (typeof Object(-1)));
}

// CHECK#12
if (Object(-1).constructor.prototype !== Number.prototype) {
  throw new Test262Error('#12: Object(-1).constructor.prototype === Number.prototype. Actual: ' + (Object(-1).constructor.prototype));
}

// CHECK#13
if (Object(Number.MIN_VALUE).valueOf() !== Number.MIN_VALUE) {
  throw new Test262Error('#13: Object(Number.MIN_VALUE).valueOf() === Number.MIN_VALUE. Actual: ' + (Object(Number.MIN_VALUE).valueOf()));
}

// CHECK#14
if (typeof Object(Number.MIN_VALUE) !== "object") {
  throw new Test262Error('#14: typeof Object(Number.MIN_VALUE) === "object". Actual: ' + (typeof Object(Number.MIN_VALUE)));
}

// CHECK#15
if (Object(Number.MIN_VALUE).constructor.prototype !== Number.prototype) {
  throw new Test262Error('#15: Object(Number.MIN_VALUE).constructor.prototype === Number.prototype. Actual: ' + (Object(Number.MIN_VALUE).constructor.prototype));
}

// CHECK#16
if (Object(Number.MAX_VALUE).valueOf() !== Number.MAX_VALUE) {
  throw new Test262Error('#16: Object(Number.MAX_VALUE).valueOf() === Number.MAX_VALUE. Actual: ' + (Object(Number.MAX_VALUE).valueOf()));
}

// CHECK#17
if (typeof Object(Number.MAX_VALUE) !== "object") {
  throw new Test262Error('#17: typeof Object(Number.MAX_VALUE) === "object". Actual: ' + (typeof Object(Number.MAX_VALUE)));
}

// CHECK#18
if (Object(Number.MAX_VALUE).constructor.prototype !== Number.prototype) {
  throw new Test262Error('#18: Object(Number.MAX_VALUE).constructor.prototype === Number.prototype. Actual: ' + (Object(Number.MAX_VALUE).constructor.prototype));
}

// CHECK#19
if (Object(Number.POSITIVE_INFINITY).valueOf() !== Number.POSITIVE_INFINITY) {
  throw new Test262Error('#19: Object(Number.POSITIVE_INFINITY).valueOf() === Number.POSITIVE_INFINITY. Actual: ' + (Object(Number.POSITIVE_INFINITY).valueOf()));
}

// CHECK#20
if (typeof Object(Number.POSITIVE_INFINITY) !== "object") {
  throw new Test262Error('#20: typeof Object(Number.POSITIVE_INFINITY) === "object". Actual: ' + (typeof Object(Number.POSITIVE_INFINITY)));
}

// CHECK#21
if (Object(Number.POSITIVE_INFINITY).constructor.prototype !== Number.prototype) {
  throw new Test262Error('#21: Object(Number.POSITIVE_INFINITY).constructor.prototype === Number.prototype. Actual: ' + (Object(Number.POSITIVE_INFINITY).constructor.prototype));
}

// CHECK#22
if (Object(Number.NEGATIVE_INFINITY).valueOf() !== Number.NEGATIVE_INFINITY) {
  throw new Test262Error('#22: Object(Number.NEGATIVE_INFINITY).valueOf() === Number.NEGATIVE_INFINITY. Actual: ' + (Object(Number.NEGATIVE_INFINITY).valueOf()));
}

// CHECK#23
if (typeof Object(Number.NEGATIVE_INFINITY) !== "object") {
  throw new Test262Error('#23: typeof Object(Number.NEGATIVE_INFINITY) === "object". Actual: ' + (typeof Object(Number.NEGATIVE_INFINITY)));
}

// CHECK#24
if (Object(Number.NEGATIVE_INFINITY).constructor.prototype !== Number.prototype) {
  throw new Test262Error('#24: Object(Number.NEGATIVE_INFINITY).constructor.prototype === Number.prototype. Actual: ' + (Object(Number.NEGATIVE_INFINITY).constructor.prototype));
}

// CHECK#25
assert.sameValue(Object(NaN).valueOf(), NaN, "Object(NaN).valueOf()");

// CHECK#26
if (typeof Object(Number.NaN) !== "object") {
  throw new Test262Error('#26: typeof Object(Number.NaN) === "object". Actual: ' + (typeof Object(Number.NaN)));
}

// CHECK#27
if (Object(Number.NaN).constructor.prototype !== Number.prototype) {
  throw new Test262Error('#27: Object(Number.NaN).constructor.prototype === Number.prototype. Actual: ' + (Object(Number.NaN).constructor.prototype));
}

// CHECK#28
if (Object(1.2345).valueOf() !== 1.2345) {
  throw new Test262Error('#28: Object(1.2345).valueOf() === 1.2345. Actual: ' + (Object(1.2345).valueOf()));
}

// CHECK#29
if (typeof Object(1.2345) !== "object") {
  throw new Test262Error('#29: typeof Object(1.2345) === "object". Actual: ' + (typeof Object(1.2345)));
}

// CHECK#30
if (Object(1.2345).constructor.prototype !== Number.prototype) {
  throw new Test262Error('#30: Object(1.2345).constructor.prototype === Number.prototype. Actual: ' + (Object(1.2345).constructor.prototype));
}

// CHECK#31
if (Object(-1.2345).valueOf() !== -1.2345) {
  throw new Test262Error('#31: Object(-1.2345).valueOf() === -1.2345. Actual: ' + (Object(-1.2345).valueOf()));
}

// CHECK#32
if (typeof Object(-1.2345) !== "object") {
  throw new Test262Error('#32: typeof Object(-1.2345) === "object". Actual: ' + (typeof Object(-1.2345)));
}

// CHECK#33
if (Object(-1.2345).constructor.prototype !== Number.prototype) {
  throw new Test262Error('#33: Object(-1.2345).constructor.prototype === Number.prototype. Actual: ' + (Object(-1.2345).constructor.prototype));
}
