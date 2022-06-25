// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-integer-indexed-exotic-objects-delete-p
description: >
  Use OrdinaryDelete if key is not a CanonicalNumericIndex
info: |
  [[Delete]] (P)

  ...
  Assert: IsPropertyKey(P) is true.
  Assert: O is an Integer-Indexed exotic object.
  If Type(P) is String, then
    Let numericIndex be ! CanonicalNumericIndexString(P).
    If numericIndex is not undefined, then
      If IsDetachedBuffer(O.[[ViewedArrayBuffer]]) is true, return true.
      If ! IsValidIntegerIndex(O, numericIndex) is false, return true.
      Return false.
    ...
  Return ? OrdinaryDelete(O, P).
flags: [onlyStrict]
includes: [testBigIntTypedArray.js]
features: [align-detached-buffer-semantics-with-web-reality, BigInt, TypedArray]
---*/

testWithBigIntTypedArrayConstructors(function(TA) {
  TypedArray.prototype.baz = "baz";
  let sample = new TA(1);

  assert.sameValue(
    delete sample.foo, true,
    'The value of `delete sample.foo` is true'
  );

  sample.foo = "foo";
  assert.sameValue(delete sample.foo, true, 'The value of `delete sample.foo` is true');

  Object.defineProperty(sample, "bar", {
    get() { return "bar"; }
  });

  assert.throws(TypeError, () => {
    delete sample.bar;
  }, '`delete sample.bar` throws TypeError');

  assert.sameValue(delete sample.baz, true, 'The value of `delete sample.baz` is true');
});
