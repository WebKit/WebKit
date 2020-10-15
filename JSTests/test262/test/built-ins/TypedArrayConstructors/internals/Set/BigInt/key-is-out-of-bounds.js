// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-integer-indexed-exotic-objects-set-p-v-receiver
description: >
  Returns false if index is out of bounds
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
  8. Let length be the value of O's [[ArrayLength]] internal slot.
  9. If index < 0 or index ≥ length, return false.
  ...
includes: [testBigIntTypedArray.js]
features: [align-detached-buffer-semantics-with-web-reality, BigInt, Reflect, TypedArray]
---*/
testWithBigIntTypedArrayConstructors(function(TA) {
  var sample = new TA([42n]);
  assert.sameValue(Reflect.set(sample, '-1', 1n), false, 'Reflect.set("new TA([42n])", "-1", 1n) must return false');
  assert.sameValue(Reflect.set(sample, '1', 1n), false, 'Reflect.set("new TA([42n])", "1", 1n) must return false');
  assert.sameValue(Reflect.set(sample, '2', 1n), false, 'Reflect.set("new TA([42n])", "2", 1n) must return false');
  assert.sameValue(sample.hasOwnProperty('-1'), false, 'sample.hasOwnProperty("-1") must return false');
  assert.sameValue(sample.hasOwnProperty('1'), false, 'sample.hasOwnProperty("1") must return false');
  assert.sameValue(sample.hasOwnProperty('2'), false, 'sample.hasOwnProperty("2") must return false');
});
