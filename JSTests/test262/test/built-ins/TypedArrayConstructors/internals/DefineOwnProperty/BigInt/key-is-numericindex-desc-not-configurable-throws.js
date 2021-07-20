// Copyright (C) 2021 Alexey Shvayka. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-integer-indexed-exotic-objects-defineownproperty-p-desc
description: >
  Throws TypeError for valid index & non-configurable descriptor.
info: |
  [[DefineOwnProperty]] ( P, Desc )

  [...]
  3. If Type(P) is String, then
    a. Let numericIndex be ! CanonicalNumericIndexString(P).
    b. If numericIndex is not undefined, then
      [...]
      ii. If Desc has a [[Configurable]] field and if Desc.[[Configurable]] is false, return false.
includes: [testBigIntTypedArray.js]
features: [align-detached-buffer-semantics-with-web-reality, BigInt, TypedArray]
---*/

testWithBigIntTypedArrayConstructors(function(TA) {
  var sample = new TA([0n]);

  assert.throws(TypeError, function() {
    Object.defineProperty(sample, "0", {
      configurable: false,
    });
  }, "partial descriptor");

  assert.throws(TypeError, function() {
    Object.defineProperty(sample, "0", {
      value: 42n,
      writable: true,
      enumerable: true,
      configurable: false,
    });
  }, "complete descriptor");

  assert.sameValue(sample[0], 0n, "side effect check");
});
