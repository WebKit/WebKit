// Copyright (C) 2016 The V8 Project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Throws a Range Error if length represents an integer < 0
info: |
  ArrayBuffer( length )

  1. If NewTarget is undefined, throw a TypeError exception.
  2. Let byteLength be ? ToIndex(length).

  ToIndex( value )

  1. If value is undefined, then
    a. Let index be 0.
  2. Else,
    a. Let integerIndex be ? ToInteger(value).
    b. If integerIndex < 0, throw a RangeError exception.
  ...
---*/

assert.throws(RangeError, function() {
  new ArrayBuffer(-1);
});

assert.throws(RangeError, function() {
  new ArrayBuffer(-1.1);
});

assert.throws(RangeError, function() {
  new ArrayBuffer(-Infinity);
});
