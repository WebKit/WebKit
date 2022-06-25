// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.copywithin
description: >
  Set values with out of bounds negative start argument.
info: |
  22.2.3.5 %TypedArray%.prototype.copyWithin (target, start [ , end ] )

  %TypedArray%.prototype.copyWithin is a distinct function that implements the
  same algorithm as Array.prototype.copyWithin as defined in 22.1.3.3 except
  that the this object's [[ArrayLength]] internal slot is accessed in place of
  performing a [[Get]] of "length" and the actual copying of values in step 12
  must be performed in a manner that preserves the bit-level encoding of the
  source data.

  ...

  22.1.3.3 Array.prototype.copyWithin (target, start [ , end ] )

  ...
  6. If relativeStart < 0, let from be max((len + relativeStart), 0); else let
  from be min(relativeStart, len).
  ...
includes: [compareArray.js, testBigIntTypedArray.js]
features: [BigInt, TypedArray]
---*/

testWithBigIntTypedArrayConstructors(function(TA) {
  assert(
    compareArray(
      new TA([0n, 1n, 2n, 3n]).copyWithin(0, -10),
      [0n, 1n, 2n, 3n]
    ),
    '[0, 1, 2, 3]).copyWithin(0, -10) -> [0, 1, 2, 3]'
  );

  assert(
    compareArray(
      new TA([1n, 2n, 3n, 4n, 5n]).copyWithin(0, -Infinity),
      [1n, 2n, 3n, 4n, 5n]
    ),
    '[1, 2, 3, 4, 5]).copyWithin(0, -Infinity) -> [1, 2, 3, 4, 5]'
  );

  assert(
    compareArray(
      new TA([0n, 1n, 2n, 3n, 4n]).copyWithin(2, -10),
      [0n, 1n, 0n, 1n, 2n]
    ),
    '[0, 1, 2, 3, 4]).copyWithin(2, -2) -> [0, 1, 0, 1, 2]'
  );

  assert(
    compareArray(
      new TA([1n, 2n, 3n, 4n, 5n]).copyWithin(2, -Infinity),
      [1n, 2n, 1n, 2n, 3n]
    ),
    '[1, 2, 3, 4, 5]).copyWithin(2, -Infinity) -> [1, 2, 1, 2, 3]'
  );

  assert(
    compareArray(
      new TA([0n, 1n, 2n, 3n, 4n]).copyWithin(10, -10),
      [0n, 1n, 2n, 3n, 4n]
    ),
    '[0, 1, 2, 3, 4]).copyWithin(10, -10) -> [0, 1, 2, 3, 4]'
  );

  assert(
    compareArray(
      new TA([1n, 2n, 3n, 4n, 5n]).copyWithin(10, -Infinity),
      [1n, 2n, 3n, 4n, 5n]
    ),
    '[1, 2, 3, 4, 5]).copyWithin(10, -Infinity) -> [1, 2, 3, 4, 5]'
  );

  assert(
    compareArray(
      new TA([0n, 1n, 2n, 3n]).copyWithin(-9, -10),
      [0n, 1n, 2n, 3n]
    ),
    '[0, 1, 2, 3].copyWithin(-9, -10) -> [0, 1, 2, 3]'
  );

  assert(
    compareArray(
      new TA([1n, 2n, 3n, 4n, 5n]).copyWithin(-9, -Infinity),
      [1n, 2n, 3n, 4n, 5n]
    ),
    '[1, 2, 3, 4, 5].copyWithin(-9, -Infinity) -> [1, 2, 3, 4, 5]'
  );
});
