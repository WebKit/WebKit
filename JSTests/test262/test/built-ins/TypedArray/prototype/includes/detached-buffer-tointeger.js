// Copyright (C) 2020 Google. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.includes
description: Throws a TypeError if this has a detached buffer after index coercion
info: |
  22.2.3.14 %TypedArray%.prototype.includes ( searchElement [ , fromIndex ] )

  This function is not generic. ValidateTypedArray is applied to the this value
  prior to evaluating the algorithm. If its result is an abrupt completion that
  exception is thrown instead of evaluating the algorithm.

  22.2.3.5.1 Runtime Semantics: ValidateTypedArray ( O )

  ...
  5. If IsDetachedBuffer(buffer) is true, throw a TypeError exception.
  ...
includes: [testTypedArray.js, detachArrayBuffer.js]
features: [TypedArray]
---*/

testWithTypedArrayConstructors(function(TA) {
  var sample = new TA(10);
  function detachAndReturnIndex(){
    $DETACHBUFFER(sample.buffer);
    return 0;
  }
  assert.throws(TypeError, function() {
    sample.includes(0, {valueOf :detachAndReturnIndex});
  });
});
