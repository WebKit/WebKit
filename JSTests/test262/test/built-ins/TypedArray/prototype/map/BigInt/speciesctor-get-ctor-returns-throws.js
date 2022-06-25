// Copyright (C) 2018 Peter Wong. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.map
description: >
  Throws if O.constructor returns a non-Object and non-undefined value
info: |
  22.2.3.19 %TypedArray%.prototype.map ( callbackfn [ , thisArg ] )

  ...
  6. Let A be ? TypedArraySpeciesCreate(O, « len »).
  ...

  22.2.4.7 TypedArraySpeciesCreate ( exemplar, argumentList )

  ...
  3. Let constructor be ? SpeciesConstructor(exemplar, defaultConstructor).
  ...

  7.3.20 SpeciesConstructor ( O, defaultConstructor )

  1. Assert: Type(O) is Object.
  2. Let C be ? Get(O, "constructor").
  3. If C is undefined, return defaultConstructor.
  4. If Type(C) is not Object, throw a TypeError exception.
  ...
includes: [testBigIntTypedArray.js]
features: [BigInt, Symbol, TypedArray]
---*/

var callbackfn = function() { return 0n; };

testWithBigIntTypedArrayConstructors(function(TA) {
  var sample = new TA([40n, 41n, 42n, 43n]);

  sample.constructor = 42;
  assert.throws(TypeError, function() {
    sample.map(callbackfn);
  }, "42");

  sample.constructor = "1";
  assert.throws(TypeError, function() {
    sample.map(callbackfn);
  }, "string");

  sample.constructor = null;
  assert.throws(TypeError, function() {
    sample.map(callbackfn);
  }, "null");

  sample.constructor = NaN;
  assert.throws(TypeError, function() {
    sample.map(callbackfn);
  }, "NaN");

  sample.constructor = false;
  assert.throws(TypeError, function() {
    sample.map(callbackfn);
  }, "false");

  sample.constructor = Symbol("1");
  assert.throws(TypeError, function() {
    sample.map(callbackfn);
  }, "symbol");
});
