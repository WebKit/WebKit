// Copyright (C) 2020 Google. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.includes
description: >
  Does not throw a TypeError if this has a detached buffer after index coercion,
  because ValidateTypedArray has already successfully completed.

info: |
  22.2.3.14 %TypedArray%.prototype.includes ( searchElement [ , fromIndex ] )

  The interpretation and use of the arguments of %TypedArray%.prototype.includes are the same as for Array.prototype.includes as defined in 22.1.3.13.

  When the includes method is called with one or two arguments, the following steps are taken:

  Let O be the this value.
  Perform ? ValidateTypedArray(O).
  Let len be O.[[ArrayLength]].
  If len is 0, return false.
  Let n be ? ToIntegerOrInfinity(fromIndex).
  ...

includes: [testTypedArray.js, detachArrayBuffer.js]
features: [align-detached-buffer-semantics-with-web-reality, TypedArray]
---*/

testWithTypedArrayConstructors(function(TA) {
  const sample = new TA(10);
  let isDetached = false;
  function valueOf(){
    $DETACHBUFFER(sample.buffer);
    isDetached = true;
    return 0;
  }
  assert.sameValue(sample.includes(0, {valueOf}), false);
  assert.sameValue(isDetached, true);
});
