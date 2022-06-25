// Copyright (C) 2018 Peter Wong. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.map
description: >
  Does not throw a TypeError if new typedArray's length >= len
info: |
  22.2.3.19 %TypedArray%.prototype.map ( callbackfn [ , thisArg ] )

  ...
  6. Let A be ? TypedArraySpeciesCreate(O, « len »).
  ...

  22.2.4.7 TypedArraySpeciesCreate ( exemplar, argumentList )

  ...
  4. Return ? TypedArrayCreate(constructor, argumentList).

  22.2.4.6 TypedArrayCreate ( constructor, argumentList )

  ...
  3. If argumentList is a List of a single Number, then
    a. If the value of newTypedArray's [[ArrayLength]] internal slot <
    argumentList[0], throw a TypeError exception.
  ...
includes: [testBigIntTypedArray.js]
features: [BigInt, Symbol.species, TypedArray]
---*/

testWithBigIntTypedArrayConstructors(function(TA) {
  var sample = new TA(2);
  var customCount, result;

  sample.constructor = {};
  sample.constructor[Symbol.species] = function() {
    return new TA(customCount);
  };

  customCount = 2;
  result = sample.map(function() { return 0n; });
  assert.sameValue(result.length, customCount, "length == count");

  customCount = 5;
  result = sample.map(function() { return 0n; });
  assert.sameValue(result.length, customCount, "length > count");
});
