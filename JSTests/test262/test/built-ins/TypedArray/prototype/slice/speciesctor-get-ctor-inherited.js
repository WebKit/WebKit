// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.slice
description: get inherited constructor on SpeciesConstructor
info: |
  22.2.3.24 %TypedArray%.prototype.slice ( start, end )

  ...
  9. Let A be ? TypedArraySpeciesCreate(O, « count »).
  ...

  22.2.4.7 TypedArraySpeciesCreate ( exemplar, argumentList )

  ...
  3. Let constructor be ? SpeciesConstructor(exemplar, defaultConstructor).
  ...

  7.3.20 SpeciesConstructor ( O, defaultConstructor )

  1. Assert: Type(O) is Object.
  2. Let C be ? Get(O, "constructor").
  3. If C is undefined, return defaultConstructor.
  ...
includes: [testTypedArray.js]
features: [TypedArray]
---*/

testWithTypedArrayConstructors(function(TA) {
  var sample = new TA([40, 41, 42, 43]);
  var calls = 0;
  var result;

  Object.defineProperty(TA.prototype, "constructor", {
    get: function() {
      calls++;
    }
  });

  result = sample.slice();

  assert.sameValue(calls, 1, "called custom ctor get accessor once");

  assert.sameValue(
    Object.getPrototypeOf(result),
    Object.getPrototypeOf(sample),
    "use defaultCtor on an undefined return - getPrototypeOf check"
  );
  assert.sameValue(
    result.constructor,
    undefined,
    "used defaultCtor but still checks the inherited .constructor"
  );

  calls = 6;
  result.constructor;
  assert.sameValue(
    calls,
    7,
    "result.constructor triggers the inherited accessor property"
  );
});
