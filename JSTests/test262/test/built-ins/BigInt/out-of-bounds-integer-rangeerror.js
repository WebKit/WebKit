// Copyright (C) 2017 Robin Templeton. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: BigInt throws a RangeError if value is not a safe integer.
esid: sec-bigint-constructor
info: |
  BigInt ( value )

  ...
  2. Let prim be ? ToPrimitive(value, hint Number).
  3. If Type(prim) is Number, return ? NumberToBigInt(prim).
  ...

  NumberToBigInt ( number )

  ...
  2. If IsSafeInteger(number) is false, throw a RangeError exception.
  ...

  IsSafeInteger ( number )

  ...
  3. Let integer be ToInteger(number).
  4. If integer is not equal to number, return false.
  5. If abs(integer) ≤ 2**53-1, return true.
  6. Otherwise, return false.
features: [BigInt]
---*/

var pos = Math.pow(2, 53);
var neg = -pos;

assert.throws(RangeError, function() {
  BigInt(pos);
});

assert.throws(RangeError, function() {
  BigInt(neg);
});
