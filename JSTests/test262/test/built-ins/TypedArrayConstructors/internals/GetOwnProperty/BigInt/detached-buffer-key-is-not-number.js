// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-integer-indexed-exotic-objects-getownproperty-p
description: >
  Does not throw on an instance with a detached buffer if key is not a number
info: |
  9.4.5.1 [[GetOwnProperty]] ( P )

  ...
  3. If Type(P) is String, then
    a. Let numericIndex be ! CanonicalNumericIndexString(P).
    b. If numericIndex is not undefined, then
      ...
  4. Return OrdinaryGetOwnProperty(O, P).
includes: [testBigIntTypedArray.js, detachArrayBuffer.js]
features: [BigInt, TypedArray]
---*/

testWithBigIntTypedArrayConstructors(function(TA) {
  var sample = new TA([42n, 43n]);
  $DETACHBUFFER(sample.buffer);

  assert.sameValue(
    Object.getOwnPropertyDescriptor(sample, "undef"),
    undefined,
    "undefined property"
  );

  // Tests for the property descriptor are defined on the tests for
  // [[DefineOwnProperty]] calls
  Object.defineProperty(sample, "foo", { value: "bar" });
  assert.sameValue(
    Object.getOwnPropertyDescriptor(sample, "foo").value,
    "bar",
    "return value from a String key"
  );
});
