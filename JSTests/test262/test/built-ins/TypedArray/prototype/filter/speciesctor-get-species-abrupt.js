// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.filter
description: >
  Returns abrupt from get @@species on found constructor
info: |
  22.2.3.9 %TypedArray%.prototype.filter ( callbackfn [ , thisArg ] )

  ...
  10. Let A be ? TypedArraySpeciesCreate(O, « captured »).
  ...

  22.2.4.7 TypedArraySpeciesCreate ( exemplar, argumentList )

  ...
  3. Let constructor be ? SpeciesConstructor(exemplar, defaultConstructor).
  ...

  7.3.20 SpeciesConstructor ( O, defaultConstructor )

  1. Assert: Type(O) is Object.
  2. Let C be ? Get(O, "constructor").
  ...
  5. Let S be ? Get(C, @@species).
  ...
includes: [testTypedArray.js]
features: [Symbol.species, TypedArray]
---*/

testWithTypedArrayConstructors(function(TA) {
  var sample = new TA(2);

  sample.constructor = {};

  Object.defineProperty(sample.constructor, Symbol.species, {
    get: function() {
      throw new Test262Error();
    }
  });

  assert.throws(Test262Error, function() {
    sample.filter(function() { return true; });
  });
});
