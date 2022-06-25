// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-integer-indexed-exotic-objects-defineownproperty-p-desc
description: >
  Returns true if key is a numeric index and Desc.[[Configurable]] is true
info: |
  9.4.5.3 [[DefineOwnProperty]] ( P, Desc)
  ...
  3. If Type(P) is String, then
    a. Let numericIndex be ! CanonicalNumericIndexString(P).
    b. If numericIndex is not undefined, then
      ...
      If Desc has a [[Value]] field, then
        Let value be Desc.[[Value]].
        Return ? IntegerIndexedElementSet(O, numericIndex, value).
  ...
includes: [testBigIntTypedArray.js]
features: [BigInt, Reflect, TypedArray]
---*/

testWithBigIntTypedArrayConstructors(function(TA) {
  var sample = new TA(2);

  assert.sameValue(
    Reflect.defineProperty(sample, "0", {
      value: 42n,
      configurable: true,
      enumerable: true,
      writable: true
    }),
    true,
    "defineProperty's result"
  );
  assert.sameValue(sample[0], 42n, "side effect check");
});
