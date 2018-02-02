// Copyright (C) 2017 Robin Templeton. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: Throws a TypeError if BigInt is called with a new target
esid: sec-bigint-constructor
info: |
  ...
  3. If Type(prim) is Number, return ? NumberToBigInt(prim).
  ...

  NumberToBigInt ( number )

  2. If IsSafeInteger(number) is false, throw a RangeError exception.

features: [BigInt]
---*/

assert.sameValue(BigInt(Number.MAX_SAFE_INTEGER), 9007199254740991n);
assert.sameValue(BigInt(-Number.MAX_SAFE_INTEGER), -9007199254740991n);
