// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from At test
  in V8's mjsunit test typedarray-resizablearraybuffer.js
features: [resizable-arraybuffer]
flags: [onlyStrict]
---*/

class MyUint8Array extends Uint8Array {
}

class MyFloat32Array extends Float32Array {
}

class MyBigInt64Array extends BigInt64Array {
}

const builtinCtors = [
  Uint8Array,
  Int8Array,
  Uint16Array,
  Int16Array,
  Uint32Array,
  Int32Array,
  Float32Array,
  Float64Array,
  Uint8ClampedArray,
  BigUint64Array,
  BigInt64Array
];

const ctors = [
  ...builtinCtors,
  MyUint8Array,
  MyFloat32Array,
  MyBigInt64Array
];

function CreateResizableArrayBuffer(byteLength, maxByteLength) {
  return new ArrayBuffer(byteLength, { maxByteLength: maxByteLength });
}

function WriteToTypedArray(array, index, value) {
  if (array instanceof BigInt64Array || array instanceof BigUint64Array) {
    array[index] = BigInt(value);
  } else {
    array[index] = value;
  }
}

function Convert(item) {
  if (typeof item == 'bigint') {
    return Number(item);
  }
  return item;
}

function TypedArrayAtHelper(ta, index) {
  const result = ta.at(index);
  return Convert(result);
}

function ArrayAtHelper(ta, index) {
  const result = Array.prototype.at.call(ta, index);
  return Convert(result);
}

function At(atHelper, oobThrows) {
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(rab, 0);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);

    // Write some data into the array.
    let ta_write = new ctor(rab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(ta_write, i, i);
    }
    assert.sameValue(atHelper(fixedLength, -1), 3);
    assert.sameValue(atHelper(lengthTracking, -1), 3);
    assert.sameValue(atHelper(fixedLengthWithOffset, -1), 3);
    assert.sameValue(atHelper(lengthTrackingWithOffset, -1), 3);

    // Shrink so that fixed length TAs go out of bounds.
    rab.resize(3 * ctor.BYTES_PER_ELEMENT);
    if (oobThrows) {
      assert.throws(TypeError, () => {
        atHelper(fixedLength, -1);
      });
      assert.throws(TypeError, () => {
        atHelper(fixedLengthWithOffset, -1);
      });
    } else {
      assert.sameValue(atHelper(fixedLength, -1), undefined);
      assert.sameValue(atHelper(fixedLengthWithOffset, -1), undefined);
    }
    assert.sameValue(atHelper(lengthTracking, -1), 2);
    assert.sameValue(atHelper(lengthTrackingWithOffset, -1), 2);

    // Shrink so that the TAs with offset go out of bounds.
    rab.resize(1 * ctor.BYTES_PER_ELEMENT);
    if (oobThrows) {
      assert.throws(TypeError, () => {
        atHelper(fixedLength, -1);
      });
      assert.throws(TypeError, () => {
        atHelper(fixedLengthWithOffset, -1);
      });
      assert.throws(TypeError, () => {
        atHelper(lengthTrackingWithOffset, -1);
      });
    } else {
      assert.sameValue(atHelper(fixedLength, -1), undefined);
      assert.sameValue(atHelper(fixedLengthWithOffset, -1), undefined);
      assert.sameValue(atHelper(lengthTrackingWithOffset, -1), undefined);
    }
    assert.sameValue(atHelper(lengthTracking, -1), 0);

    // Grow so that all TAs are back in-bounds. New memory is zeroed.
    rab.resize(6 * ctor.BYTES_PER_ELEMENT);
    assert.sameValue(atHelper(fixedLength, -1), 0);
    assert.sameValue(atHelper(lengthTracking, -1), 0);
    assert.sameValue(atHelper(fixedLengthWithOffset, -1), 0);
    assert.sameValue(atHelper(lengthTrackingWithOffset, -1), 0);
  }
}

At(TypedArrayAtHelper, true);
At(ArrayAtHelper, false);
