// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.filter
description: >
  Integer indexed values are not cached before interaction
info: |
  22.2.3.9 %TypedArray%.prototype.filter ( callbackfn [ , thisArg ] )

  ...
  9. Repeat, while k < len
    ...
    c. Let selected be ToBoolean(? Call(callbackfn, T, « kValue, k, O »)).
  ...
includes: [testTypedArray.js]
features: [TypedArray]
---*/

testWithTypedArrayConstructors(function(TA) {
  var sample = new TA([42, 43, 44]);

  sample.filter(function(v, i) {
    if (i < sample.length - 1) {
      sample[i+1] = 42;
    }

    assert.sameValue(
      v, 42, "method does not cache values before callbackfn calls"
    );
  });
});
