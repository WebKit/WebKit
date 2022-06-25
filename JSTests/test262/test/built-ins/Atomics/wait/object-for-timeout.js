// Copyright (C) 2018 Amal Hussein. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-atomics.wait
description: >
  Throws a TypeError if index arg can not be converted to an Integer
info: |
  Atomics.wait( typedArray, index, value, timeout )

  4. Let q be ? ToNumber(timeout).

    Object -> Apply the following steps:

      Let primValue be ? ToPrimitive(argument, hint Number).
      Return ? ToNumber(primValue).

features: [Atomics, SharedArrayBuffer, Symbol, Symbol.toPrimitive, TypedArray]
flags: [CanBlockIsTrue]
---*/

const i32a = new Int32Array(
  new SharedArrayBuffer(Int32Array.BYTES_PER_ELEMENT * 4)
);

const valueOf = {
  valueOf: function() {
    return 0;
  }
};

const toString = {
  toString: function() {
    return "0";
  }
};

const toPrimitive = {
  [Symbol.toPrimitive]: function() {
    return 0;
  }
};

assert.sameValue(
  Atomics.wait(i32a, 0, 0, valueOf),
  'timed-out',
  'Atomics.wait(i32a, 0, 0, valueOf) returns "timed-out"'
);
assert.sameValue(
  Atomics.wait(i32a, 0, 0, toString),
  'timed-out',
  'Atomics.wait(i32a, 0, 0, toString) returns "timed-out"'
);
assert.sameValue(
  Atomics.wait(i32a, 0, 0, toPrimitive),
  'timed-out',
  'Atomics.wait(i32a, 0, 0, toPrimitive) returns "timed-out"'
);
