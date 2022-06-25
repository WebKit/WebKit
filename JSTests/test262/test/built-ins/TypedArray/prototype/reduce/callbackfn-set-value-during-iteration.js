// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-%typedarray%.prototype.reduce
description: >
  Integer indexed values changed during iteration
info: |
  22.2.3.20 %TypedArray%.prototype.reduce ( callbackfn [ , initialValue ] )

  %TypedArray%.prototype.reduce is a distinct function that implements the same
  algorithm as Array.prototype.reduce as defined in 22.1.3.19 except that the
  this object's [[ArrayLength]] internal slot is accessed in place of performing
  a [[Get]] of "length".

  22.1.3.19 Array.prototype.reduce ( callbackfn [ , initialValue ] )
includes: [testTypedArray.js]
features: [Reflect.set, TypedArray]
---*/

testWithTypedArrayConstructors(function(TA) {
  var sample = new TA([42, 43, 44]);
  var newVal = 0;

  sample.reduce(function(acc, val, i) {
    if (i > 0) {
      assert.sameValue(
        sample[i - 1], newVal - 1,
        "get the changed value during the loop"
      );
      assert.sameValue(
        Reflect.set(sample, 0, 7),
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
  }, 0);

  assert.sameValue(sample[0], 7, "changed values after iteration [0] == 7");
  assert.sameValue(sample[1], 1, "changed values after iteration [1] == 1");
  assert.sameValue(sample[2], 2, "changed values after iteration [2] == 2");
});
