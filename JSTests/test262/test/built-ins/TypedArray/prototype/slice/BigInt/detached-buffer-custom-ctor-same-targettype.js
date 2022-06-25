// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.slice
description: >
  Throws a TypeError if _O_.[[ViewedArrayBuffer]] is detached during create with custom constructor.
info: |
  %TypedArray%.prototype.slice ( start, end )

    Let A be ? TypedArraySpeciesCreate(O, « count »).

  TypedArraySpeciesCreate ( exemplar, argumentList )

    Let result be ? TypedArrayCreate(constructor, argumentList).

  TypedArrayCreate ( constructor, argumentList )

    Let newTypedArray be ? Construct(constructor, argumentList).
    Perform ? ValidateTypedArray(newTypedArray).

  ValidateTypedArray ( O )
    The abstract operation ValidateTypedArray takes argument O. It performs the following steps when called:

    Perform ? RequireInternalSlot(O, [[TypedArrayName]]).
    Assert: O has a [[ViewedArrayBuffer]] internal slot.
    Let buffer be O.[[ViewedArrayBuffer]].
    If IsDetachedBuffer(buffer) is true, throw a TypeError exception.
    ...

includes: [testBigIntTypedArray.js, detachArrayBuffer.js]
features: [align-detached-buffer-semantics-with-web-reality, BigInt, Symbol.species, TypedArray]
---*/

testWithBigIntTypedArrayConstructors(function(TA) {
  let counter = 0;
  let sample = new TA(1);

  sample.constructor = {};
  sample.constructor[Symbol.species] = function(count) {
    counter++;
    $DETACHBUFFER(sample.buffer);
    return new TA(count);
  };

  assert.throws(TypeError, function() {
    counter++;
    sample.slice();
  }, '`sample.slice()` throws TypeError');

  assert.sameValue(counter, 2, 'The value of `counter` is 2');
});
