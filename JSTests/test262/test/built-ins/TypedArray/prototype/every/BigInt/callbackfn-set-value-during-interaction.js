// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
esid: sec-%typedarray%.prototype.every
description: >
  Integer indexed values changed during iteration
info: |
  22.2.3.7 %TypedArray%.prototype.every ( callbackfn [ , thisArg ] )

  %TypedArray%.prototype.every is a distinct function that implements the same
  algorithm as Array.prototype.every as defined in 22.1.3.5 except that the this
  object's [[ArrayLength]] internal slot is accessed in place of performing a
  [[Get]] of "length".

  22.1.3.5 Array.prototype.every ( callbackfn [ , thisArg ] )

  ...
  6. Repeat, while k < len
    ..
    c. If kPresent is true, then
      i. Let kValue be ? Get(O, Pk).
      ii. Let testResult be ToBoolean(? Call(callbackfn, T, « kValue, k, O »)).
  ...
includes: [testBigIntTypedArray.js]
features: [BigInt, Reflect.set, TypedArray]
---*/

testWithBigIntTypedArrayConstructors(function(TA) {
  var sample = new TA([42n, 43n, 44n]);
  var newVal = 0n;

  sample.every(function(val, i) {
    if (i > 0) {
      assert.sameValue(
        sample[i - 1], newVal - 1n,
        "get the changed value during the loop"
      );
      assert.sameValue(
        Reflect.set(sample, 0, 7n),
        true,
        "re-set a value for sample[0]"
      );
    }
    assert.sameValue(
      Reflect.set(sample, i, newVal),
      true,
      "set value during iteration"
    );

    newVal++;

    return true;
  });

  assert.sameValue(sample[0], 7n, "changed values after iteration [0] == 7");
  assert.sameValue(sample[1], 1n, "changed values after iteration [1] == 1");
  assert.sameValue(sample[2], 2n, "changed values after iteration [2] == 2");
});
