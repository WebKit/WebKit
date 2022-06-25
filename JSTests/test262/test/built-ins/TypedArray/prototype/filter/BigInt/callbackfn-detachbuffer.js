// Copyright (C) 2016 the V8 project authors. All rights reserved.
// Copyright (C) 2021 Apple Inc. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.filter
description: >
  Instance buffer can be detached during loop
info: |
  22.2.3.9 %TypedArray%.prototype.filter ( callbackfn [ , thisArg ] )

  ...
  9. Repeat, while k < len
    ...
    b. Let kValue be ? Get(O, Pk).
    c. Let selected be ToBoolean(? Call(callbackfn, T, « kValue, k, O »)).
  ...
includes: [detachArrayBuffer.js, testBigIntTypedArray.js]
features: [BigInt, TypedArray]
---*/

testWithBigIntTypedArrayConstructors(function(TA) {
  var loops = 0;
  var sample = new TA(2);

  sample.filter(function() {
    var flag = true;
    if (loops === 0) {
      $DETACHBUFFER(sample.buffer);
    } else {
      flag = false;
    }
    loops++;
    return flag;
  });

  assert.sameValue(loops, 2);
});
