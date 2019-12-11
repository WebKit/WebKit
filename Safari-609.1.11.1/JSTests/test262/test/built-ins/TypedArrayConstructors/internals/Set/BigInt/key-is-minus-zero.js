// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-integer-indexed-exotic-objects-set-p-v-receiver
description: >
  Returns false if index is -0
info: |
  [[Set]] ( P, V, Receiver)

  ...
  2. If Type(P) is String, then
    a. Let numericIndex be ! CanonicalNumericIndexString(P).
    b. If numericIndex is not undefined, then
      i. Return ? IntegerIndexedElementSet(O, numericIndex, V).
  ...

  IntegerIndexedElementSet ( O, index, value )

  5. If arrayTypeName is "BigUint64Array" or "BigInt64Array", let numValue be ? ToBigInt(value).
  ...
  10. If index = -0, return false.
includes: [testBigIntTypedArray.js]
features: [BigInt, Reflect, TypedArray]
---*/

testWithBigIntTypedArrayConstructors(function(TA) {
  var sample = new TA([42n]);

  assert.sameValue(Reflect.set(sample, "-0", 1n), false, "-0");
  assert.sameValue(sample.hasOwnProperty("-0"), false, "has no property [-0]");
});
