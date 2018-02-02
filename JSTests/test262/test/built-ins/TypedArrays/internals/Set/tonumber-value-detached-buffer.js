// Copyright (C) 2017 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-integer-indexed-exotic-objects-set-p-v-receiver
description: >
    Setting a typed array element to a value that, when converted to the typed
    array element type, detaches the typed array's underlying buffer, should
    throw a TypeError and not modify the typed array.
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
  15. Perform SetValueInBuffer(buffer, indexedPosition, elementType, numValue).
  16. Return true.
includes: [testTypedArray.js, detachArrayBuffer.js]
features: [Reflect, TypedArray]
---*/

testWithTypedArrayConstructors(function(TA) {
  var ta = new TA([17]);

  assert.throws(TypeError, function() {
    Reflect.set(ta, 0, {
      valueOf: function() {
        $262.detachArrayBuffer(ta.buffer);
        return 42;
      }
    });
  },
  "detaching a ArrayBuffer during setting an element of a typed array " +
  "viewing it should throw");

  assert.throws(TypeError, function() {
    ta[0];
  });
});
