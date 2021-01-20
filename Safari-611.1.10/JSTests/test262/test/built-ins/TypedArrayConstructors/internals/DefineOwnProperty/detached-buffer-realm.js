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

  IntegerIndexedElementSet ( O, index, value )

  Assert: O is an Integer-Indexed exotic object.
  If O.[[ContentType]] is BigInt, let numValue be ? ToBigInt(value).
  Otherwise, let numValue be ? ToNumber(value).
  Let buffer be O.[[ViewedArrayBuffer]].
  If IsDetachedBuffer(buffer) is false and ! IsValidIntegerIndex(O, index) is true, then
    Let offset be O.[[ByteOffset]].
    Let arrayTypeName be the String value of O.[[TypedArrayName]].
    Let elementSize be the Element Size value specified in Table 62 for arrayTypeName.
    Let indexedPosition be (ℝ(index) × elementSize) + offset.
    Let elementType be the Element Type value in Table 62 for arrayTypeName.
    Perform SetValueInBuffer(buffer, indexedPosition, elementType, numValue, true, Unordered).
  Return NormalCompletion(undefined).

includes: [testTypedArray.js, detachArrayBuffer.js]
features: [align-detached-buffer-semantics-with-web-reality, cross-realm, Reflect, TypedArray]
---*/

var other = $262.createRealm().global;
var desc = {
  value: 0,
  configurable: true,
  enumerable: true,
  writable: true
};

testWithTypedArrayConstructors(function(TA) {
  var OtherTA = other[TA.name];
  var sample = new OtherTA(1);

  $DETACHBUFFER(sample.buffer);

  assert.sameValue(
    Reflect.defineProperty(sample, '0', desc),
    false,
    'Reflect.defineProperty(sample, "0", {value: 0, configurable: true, enumerable: true, writable: true}) must return false'
  );
});
