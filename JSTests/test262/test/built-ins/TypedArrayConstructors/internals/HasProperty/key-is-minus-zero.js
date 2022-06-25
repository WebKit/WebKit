// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-integer-indexed-exotic-objects-hasproperty-p
description: Return false if P's value is "-0"
info: |
  9.4.5.2 [[HasProperty]](P)

  ...
  3. If Type(P) is String, then
    a. Let numericIndex be ! CanonicalNumericIndexString(P).
    b. If numericIndex is not undefined, then
      ...
      iii. If ! IsValidIntegerIndex(O, numericIndex) is false, return false.
  ...
includes: [testTypedArray.js]
features: [align-detached-buffer-semantics-with-web-reality, Reflect, TypedArray]
---*/


// Prevents false positives using OrdinaryHasProperty
TypedArray.prototype["-0"] = "test262";

testWithTypedArrayConstructors(function(TA) {
  var sample = new TA(1);

  assert.sameValue(Reflect.has(sample, "-0"), false, 'Reflect.has(sample, "-0") must return false');
});
