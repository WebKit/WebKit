// Copyright (C) 2017 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-integer-indexed-exotic-objects-hasproperty-p
description: >
  "Infinity" is a canonical numeric string, test with access on detached buffer.
info: |
  9.4.5.2 [[HasProperty]]( P )
  ...
  3. If Type(P) is String, then
    a. Let numericIndex be ! CanonicalNumericIndexString(P).
    b. If numericIndex is not undefined, then
      i. Let buffer be O.[[ViewedArrayBuffer]].
      ii. If IsDetachedBuffer(buffer) is true, throw a TypeError exception.
      ...

  7.1.16 CanonicalNumericIndexString ( argument )
    ...
    3. Let n be ! ToNumber(argument).
    4. If SameValue(! ToString(n), argument) is false, return undefined.
    5. Return n.

flags: [noStrict]
includes: [testTypedArray.js, detachArrayBuffer.js]
features: [TypedArray]
---*/

testWithTypedArrayConstructors(function(TA) {
  var sample = new TA(0);
  $DETACHBUFFER(sample.buffer);

  assert.throws(TypeError, function() {
    with (sample) Infinity;
  });
});
