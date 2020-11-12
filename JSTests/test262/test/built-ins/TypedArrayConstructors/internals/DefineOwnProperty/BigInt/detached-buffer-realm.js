// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-integer-indexed-exotic-objects-defineownproperty-p-desc
description: >
  Returns false if this has valid numeric index and a detached buffer
  (honoring the Realm of the current execution context)
info: |
  9.4.5.3 [[DefineOwnProperty]] ( P, Desc)
  ...
  3. If Type(P) is String, then
    a. Let numericIndex be ! CanonicalNumericIndexString(P).
    b. If numericIndex is not undefined, then
      ...
      xi. If Desc has a [[Value]] field, then
        1. Let value be Desc.[[Value]].
        2. Return ? IntegerIndexedElementSet(O, intIndex, value).
  ...

  9.4.5.9 IntegerIndexedElementSet ( O, index, value )

  ...
  Let buffer be O.[[ViewedArrayBuffer]].
  If IsDetachedBuffer(buffer) is true, return false.

includes: [testBigIntTypedArray.js, detachArrayBuffer.js]
features: [align-detached-buffer-semantics-with-web-reality, BigInt, cross-realm, Reflect, TypedArray]
---*/
var other = $262.createRealm().global;

var desc = {
  value: 0n,
  configurable: true,
  enumerable: true,
  writable: true
};

testWithBigIntTypedArrayConstructors(function(TA) {
  var OtherTA = other[TA.name];
  var sample = new OtherTA(1);
  $DETACHBUFFER(sample.buffer);

  assert.sameValue(
    Reflect.defineProperty(sample, '0', desc),
    false,
    'Reflect.defineProperty(sample, "0", {value: 0n, configurable: true, enumerable: true, writable: true} ) must return false'
  );
});
