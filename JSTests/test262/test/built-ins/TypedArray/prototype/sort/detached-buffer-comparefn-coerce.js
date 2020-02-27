// Copyright (C) 2020 Google. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.sort
description: >
  SECURITY Throws a TypeError if coercion of the comparefn return value
  detaches the object buffer
info: |
  22.2.3.26 %TypedArray%.prototype.sort ( comparefn )

  When the TypedArray SortCompare abstract operation is called with two
  arguments x and y, the following steps are taken:

  ...
  2. If the argument comparefn is not undefined, then
    a. Let v be ? ToNumber(? Call(comparefn, undefined, « x, y »)).
    b. If IsDetachedBuffer(buffer) is true, throw a TypeError exception.
    ...
  ...
includes: [testTypedArray.js, detachArrayBuffer.js]
features: [TypedArray]
---*/

testWithTypedArrayConstructors(function(TA) {
  var sample = new TA(4);
  var calls = 0;
  var convertfn = function(){
    $DETACHBUFFER(sample.buffer);
    return 1;
  }
  var comparefn = function() {
    if (calls > 0) {
      throw new Test262Error();
    }
    calls++;
    return {valueOf : convertfn}
  };

  assert.throws(TypeError, function() {
    sample.sort(comparefn);
  }, "Coercion that detaches buffer should throw TypeError");

  assert.sameValue(calls, 1);
});
