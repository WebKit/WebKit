// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-integer-indexed-exotic-objects-set-p-v-receiver
description: >
  Returns false if index is not integer
info: |
  9.4.5.5 [[Set]] ( P, V, Receiver)

  ...
  2. If Type(P) is String, then
    a. Let numericIndex be ! CanonicalNumericIndexString(P).
    b. If numericIndex is not undefined, then
      i. Return ? IntegerIndexedElementSet(O, numericIndex, V).
  ...

  9.4.5.9 IntegerIndexedElementSet ( O, index, value )

  ...
  6. If IsInteger(index) is false, return false.
  ...
includes: [testBigIntTypedArray.js]
features: [align-detached-buffer-semantics-with-web-reality, BigInt, Reflect, TypedArray]
---*/
testWithBigIntTypedArrayConstructors(function(TA) {
  var sample = new TA([42n]);
  assert.sameValue(Reflect.set(sample, '1.1', 1n), false, 'Reflect.set("new TA([42n])", "1.1", 1n) must return false');

  assert.sameValue(
    Reflect.set(sample, '0.0001', 1n),
    false,
    'Reflect.set("new TA([42n])", "0.0001", 1n) must return false'
  );

  assert.sameValue(sample.hasOwnProperty('1.1'), false, 'sample.hasOwnProperty("1.1") must return false');
  assert.sameValue(sample.hasOwnProperty('0.0001'), false, 'sample.hasOwnProperty("0.0001") must return false');
});
