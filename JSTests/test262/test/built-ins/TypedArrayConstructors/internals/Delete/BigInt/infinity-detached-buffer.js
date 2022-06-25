// Copyright (C) 2020 Rick Waldron. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-integerindexedelementget
description: >
  "Infinity" is a canonical numeric string. Returns true when deleting any property if buffer is detached.
info: |
  [[Delete]] ( P, Receiver )
  ...
  Assert: IsPropertyKey(P) is true.
  Assert: O is an Integer-Indexed exotic object.
  If Type(P) is String, then
    Let numericIndex be ! CanonicalNumericIndexString(P).
    If numericIndex is not undefined, then
      If IsDetachedBuffer(O.[[ViewedArrayBuffer]]) is true, return true.

  CanonicalNumericIndexString ( argument )
    ...
    Let n be ! ToNumber(argument).
    If SameValue(! ToString(n), argument) is false, return undefined.
    Return n.

includes: [testBigIntTypedArray.js, detachArrayBuffer.js]
features: [align-detached-buffer-semantics-with-web-reality, BigInt, TypedArray]
---*/

testWithBigIntTypedArrayConstructors(function(TA) {
  var sample = new TA(1);
  $DETACHBUFFER(sample.buffer);

  assert.sameValue(delete sample.Infinity, true, 'The value of `delete sample.Infinity` is true');
});
