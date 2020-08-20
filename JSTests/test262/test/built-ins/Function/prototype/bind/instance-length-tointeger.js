// Copyright (C) 2020 Alexey Shvayka. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-function.prototype.bind
description: >
  "length" value of a bound function is non-negative integer.
  ToInteger is performed on "length" value of target function.
info: |
  Function.prototype.bind ( thisArg, ...args )

  [...]
  5. Let targetHasLength be ? HasOwnProperty(Target, "length").
  6. If targetHasLength is true, then
    a. Let targetLen be ? Get(Target, "length").
    b. If Type(targetLen) is not Number, let L be 0.
    c. Else,
      i. Set targetLen to ! ToInteger(targetLen).
      ii. Let L be the larger of 0 and the result of targetLen minus the number of elements of args.
  7. Else, let L be 0.
  8. Perform ! SetFunctionLength(F, L).
  [...]

  ToInteger ( argument )

  1. Let number be ? ToNumber(argument).
  2. If number is NaN, +0, or -0, return +0.
  3. If number is +∞ or -∞, return number.
  4. Let integer be the Number value that is the same sign as number and whose magnitude is floor(abs(number)).
  5. If integer is -0, return +0.
  6. Return integer.
---*/

function fn() {}

Object.defineProperty(fn, "length", {value: NaN});
assert.sameValue(fn.bind().length, 0);

Object.defineProperty(fn, "length", {value: -0});
assert.sameValue(fn.bind().length, 0);

Object.defineProperty(fn, "length", {value: Infinity});
assert.sameValue(fn.bind().length, Infinity);

Object.defineProperty(fn, "length", {value: -Infinity});
assert.sameValue(fn.bind().length, 0);

Object.defineProperty(fn, "length", {value: 3.66});
assert.sameValue(fn.bind().length, 3);

Object.defineProperty(fn, "length", {value: -0.77});
assert.sameValue(fn.bind().length, 0);
