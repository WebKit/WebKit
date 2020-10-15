// Copyright (C) 2017 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-integer-indexed-exotic-objects-set-p-v-receiver
description: >
  Setting a typed array element to a value that, when converted to the typed
  array element type, detaches the typed array's underlying buffer, should return false.
info: |
  9.4.5.5 [[Set]] ( P, V, Receiver)

  ...
  2. If Type(P) is String, then
    a. Let numericIndex be ! CanonicalNumericIndexString(P).
    b. If numericIndex is not undefined, then
      i. Return ? IntegerIndexedElementSet(O, numericIndex, V).
  ...

  9.4.5.9 IntegerIndexedElementSet ( O, index, value )

  Assert: O is an Integer-Indexed exotic object.
  Assert: Type(index) is Number.
  If O.[[ContentType]] is BigInt, let numValue be ? ToBigInt(value).
  Otherwise, let numValue be ? ToNumber(value).
  Let buffer be O.[[ViewedArrayBuffer]].
  If IsDetachedBuffer(buffer) is true, return false.

includes: [testTypedArray.js, detachArrayBuffer.js]
features: [align-detached-buffer-semantics-with-web-reality, Reflect, TypedArray]
---*/

testWithTypedArrayConstructors(function(TA) {
  let ta = new TA(1);
  let result = Reflect.set(ta, 0, {
    valueOf() {
      $DETACHBUFFER(ta.buffer);
      return 42;
    }
  });

  assert.sameValue(result, false);
  assert.sameValue(ta[0], false);
});
