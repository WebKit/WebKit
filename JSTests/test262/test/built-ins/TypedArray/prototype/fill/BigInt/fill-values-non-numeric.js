// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.fill
description: >
  Fills all the elements with non numeric values values.
info: |
  %TypedArray%.prototype.fill ( value [ , start [ , end ] ] )

  Let O be the this value.
  Perform ? ValidateTypedArray(O).
  Let len be O.[[ArrayLength]].
  If O.[[ContentType]] is BigInt, set value to ? ToBigInt(value).
  Otherwise, set value to ? ToNumber(value).
  Let relativeStart be ? ToIntegerOrInfinity(start).
  If relativeStart is -Infinity, let k be 0.
  Else if relativeStart < 0, let k be max(len + relativeStart, 0).
  Else, let k be min(relativeStart, len).
  If end is undefined, let relativeEnd be len; else let relativeEnd be ? ToIntegerOrInfinity(end).
  If relativeEnd is -Infinity, let final be 0.
  Else if relativeEnd < 0, let final be max(len + relativeEnd, 0).
  Else, let final be min(relativeEnd, len).
  If IsDetachedBuffer(O.[[ViewedArrayBuffer]]) is true, throw a TypeError exception.
  Repeat, while k < final,
    Let Pk be ! ToString(F(k)).
    Perform ! Set(O, Pk, value, true).
    Set k to k + 1.
  Return O.

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

includes: [testBigIntTypedArray.js]
features: [BigInt, TypedArray]
---*/

testWithBigIntTypedArrayConstructors(function(TA) {
  var sample;

  sample = new TA([42n]);
  sample.fill(false);
  assert.sameValue(sample[0], 0n, "false => 0");

  sample = new TA([42n]);
  sample.fill(true);
  assert.sameValue(sample[0], 1n, "true => 1");

  sample = new TA([42n]);
  sample.fill("7");
  assert.sameValue(sample[0], 7n, "string conversion");

  sample = new TA([42n]);
  sample.fill({
    toString: function() {
      return "1";
    },
    valueOf: function() {
      return 7n;
    }
  });
  assert.sameValue(sample[0], 7n, "object valueOf conversion before toString");

  sample = new TA([42n]);
  sample.fill({
    toString: function() {
      return "7";
    }
  });
  assert.sameValue(sample[0], 7n, "object toString when valueOf is absent");

});
