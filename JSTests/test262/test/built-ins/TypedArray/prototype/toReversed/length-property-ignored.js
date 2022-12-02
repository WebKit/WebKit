// Copyright (C) 2021 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-%typedarray%.prototype.toReversed
description: >
  %TypedArray%.prototype.toReversed does not read a "length" property
info: |
  %TypedArray%.prototype.toReversed ( )

  ...
  3. Let length be O.[[ArrayLength]].
  ...
includes: [testTypedArray.js, compareArray.js]
features: [TypedArray, change-array-by-copy]
---*/

testWithTypedArrayConstructors(TA => {
  var ta = new TA([0, 1, 2]);
  Object.defineProperty(ta, "length", { value: 2 })
  var res = ta.toReversed();
  assert.compareArray(res, [2, 1, 0]);
  assert.sameValue(res.length, 3);

  ta = new TA([0, 1, 2]);
  Object.defineProperty(ta, "length", { value: 5 });
  res = ta.toReversed();
  assert.compareArray(res, [2, 1, 0]);
  assert.sameValue(res.length, 3);
});

function setLength(length) {
    Object.defineProperty(TypedArray.prototype, "length", {
        get: () => length,
    });
}

testWithTypedArrayConstructors(TA => {
  var ta = new TA([0, 1, 2]);

  setLength(2);
  var res = ta.toReversed();
  setLength(3);
  assert.compareArray(res, [2, 1, 0]);

  setLength(5);
  res = ta.toReversed();
  setLength(3);
  assert.compareArray(res, [2, 1, 0]);
});
