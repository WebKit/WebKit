// Copyright 2023 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.copywithin
description: >
  TypedArray.p.copyWithin behaves correctly when argument coercion shrinks the receiver
includes: [compareArray.js, resizableArrayBufferUtils.js]
features: [resizable-arraybuffer]
---*/

for (let ctor of ctors) {
  const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
  const fixedLength = new ctor(rab, 0, 4);
  const evil = {
    valueOf: () => {
      rab.resize(2 * ctor.BYTES_PER_ELEMENT);
      return 2;
    }
  };
  assert.throws(TypeError, () => {
    fixedLength.copyWithin(evil, 0, 1);
  });
  rab.resize(4 * ctor.BYTES_PER_ELEMENT);
  assert.throws(TypeError, () => {
    fixedLength.copyWithin(0, evil, 3);
  });
  rab.resize(4 * ctor.BYTES_PER_ELEMENT);
  assert.throws(TypeError, () => {
    fixedLength.copyWithin(0, 1, evil);
  });
}
for (let ctor of ctors) {
  const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
  const lengthTracking = new ctor(rab);
  for (let i = 0; i < 4; ++i) {
    lengthTracking[i] = MayNeedBigInt(lengthTracking, i);
  }
  // [0, 1, 2, 3]
  //        ^
  //        target
  // ^
  // start
  const evil = {
    valueOf: () => {
      rab.resize(3 * ctor.BYTES_PER_ELEMENT);
      return 2;
    }
  };
  lengthTracking.copyWithin(evil, 0);
  assert.compareArray(ToNumbers(lengthTracking), [
    0,
    1,
    0
  ]);
}
for (let ctor of ctors) {
  const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
  const lengthTracking = new ctor(rab);
  for (let i = 0; i < 4; ++i) {
    lengthTracking[i] = MayNeedBigInt(lengthTracking, i);
  }
  // [0, 1, 2, 3]
  //        ^
  //        start
  // ^
  // target
  const evil = {
    valueOf: () => {
      rab.resize(3 * ctor.BYTES_PER_ELEMENT);
      return 2;
    }
  };
  lengthTracking.copyWithin(0, evil);
  assert.compareArray(ToNumbers(lengthTracking), [
    2,
    1,
    2
  ]);
}
