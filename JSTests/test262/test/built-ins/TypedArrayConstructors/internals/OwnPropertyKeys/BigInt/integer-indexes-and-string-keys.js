// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-integer-indexed-exotic-objects-ownpropertykeys
description: >
  Return integer index + non numeric string keys
info: |
  9.4.5.6 [[OwnPropertyKeys]] ()

  ...
  3. Let len be the value of O's [[ArrayLength]] internal slot.
  4. For each integer i starting with 0 such that i < len, in ascending order,
    a. Add ! ToString(i) as the last element of keys.
  ...
includes: [testBigIntTypedArray.js, compareArray.js]
features: [BigInt, Reflect, TypedArray]
---*/

TypedArray.prototype[3] = 42;
TypedArray.prototype.bar = 42;

testWithBigIntTypedArrayConstructors(function(TA) {
  var sample1 = new TA([42n, 42n, 42n]);
  sample1.test262 = 42;
  sample1.ecma262 = 42;
  var result1 = Reflect.ownKeys(sample1);
  assert(
    compareArray(result1, ["0", "1", "2", "test262", "ecma262"]),
    "result1"
  );

  var sample2 = new TA(4).subarray(2);
  sample2.test262 = 42;
  sample2.ecma262 = 42;
  var result2 = Reflect.ownKeys(sample2);
  assert(
    compareArray(result2, ["0", "1", "test262", "ecma262"]),
    "result2"
  );
});
