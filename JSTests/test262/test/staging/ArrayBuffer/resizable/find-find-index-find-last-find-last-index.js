// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from FindFindIndexFindLastFindLastIndex test
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

function TypedArrayFindHelper(ta, p) {
  return ta.find(p);
}

function ArrayFindHelper(ta, p) {
  return Array.prototype.find.call(ta, p);
}

function TypedArrayFindIndexHelper(ta, p) {
  return ta.findIndex(p);
}

function ArrayFindIndexHelper(ta, p) {
  return Array.prototype.findIndex.call(ta, p);
}

function TypedArrayFindLastHelper(ta, p) {
  return ta.findLast(p);
}

function ArrayFindLastHelper(ta, p) {
  return Array.prototype.findLast.call(ta, p);
}

function TypedArrayFindLastIndexHelper(ta, p) {
  return ta.findLastIndex(p);
}

function ArrayFindLastIndexHelper(ta, p) {
  return Array.prototype.findLastIndex.call(ta, p);
}

function FindFindIndexFindLastFindLastIndex(findHelper, findIndexHelper, findLastHelper, findLastIndexHelper, oobThrows) {
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(rab, 0);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);

    // Write some data into the array.
    const taWrite = new ctor(rab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, 2 * i);
    }

    // Orig. array: [0, 2, 4, 6]
    //              [0, 2, 4, 6] << fixedLength
    //                    [4, 6] << fixedLengthWithOffset
    //              [0, 2, 4, 6, ...] << lengthTracking
    //                    [4, 6, ...] << lengthTrackingWithOffset

    function isTwoOrFour(n) {
      return n == 2 || n == 4;
    }
    assert.sameValue(Number(findHelper(fixedLength, isTwoOrFour)), 2);
    assert.sameValue(Number(findHelper(fixedLengthWithOffset, isTwoOrFour)), 4);
    assert.sameValue(Number(findHelper(lengthTracking, isTwoOrFour)), 2);
    assert.sameValue(Number(findHelper(lengthTrackingWithOffset, isTwoOrFour)), 4);
    assert.sameValue(findIndexHelper(fixedLength, isTwoOrFour), 1);
    assert.sameValue(findIndexHelper(fixedLengthWithOffset, isTwoOrFour), 0);
    assert.sameValue(findIndexHelper(lengthTracking, isTwoOrFour), 1);
    assert.sameValue(findIndexHelper(lengthTrackingWithOffset, isTwoOrFour), 0);
    assert.sameValue(Number(findLastHelper(fixedLength, isTwoOrFour)), 4);
    assert.sameValue(Number(findLastHelper(fixedLengthWithOffset, isTwoOrFour)), 4);
    assert.sameValue(Number(findLastHelper(lengthTracking, isTwoOrFour)), 4);
    assert.sameValue(Number(findLastHelper(lengthTrackingWithOffset, isTwoOrFour)), 4);
    assert.sameValue(findLastIndexHelper(fixedLength, isTwoOrFour), 2);
    assert.sameValue(findLastIndexHelper(fixedLengthWithOffset, isTwoOrFour), 0);
    assert.sameValue(findLastIndexHelper(lengthTracking, isTwoOrFour), 2);
    assert.sameValue(findLastIndexHelper(lengthTrackingWithOffset, isTwoOrFour), 0);

    // Shrink so that fixed length TAs go out of bounds.
    rab.resize(3 * ctor.BYTES_PER_ELEMENT);

    // Orig. array: [0, 2, 4]
    //              [0, 2, 4, ...] << lengthTracking
    //                    [4, ...] << lengthTrackingWithOffset

    if (oobThrows) {
      assert.throws(TypeError, () => {
        findHelper(fixedLength, isTwoOrFour);
      });
      assert.throws(TypeError, () => {
        findIndexHelper(fixedLength, isTwoOrFour);
      });
      assert.throws(TypeError, () => {
        findLastHelper(fixedLength, isTwoOrFour);
      });
      assert.throws(TypeError, () => {
        findLastIndexHelper(fixedLength, isTwoOrFour);
      });
      assert.throws(TypeError, () => {
        findHelper(fixedLengthWithOffset, isTwoOrFour);
      });
      assert.throws(TypeError, () => {
        findIndexHelper(fixedLengthWithOffset, isTwoOrFour);
      });
      assert.throws(TypeError, () => {
        findLastHelper(fixedLengthWithOffset, isTwoOrFour);
      });
      assert.throws(TypeError, () => {
        findLastIndexHelper(fixedLengthWithOffset, isTwoOrFour);
      });
    } else {
      assert.sameValue(findHelper(fixedLength, isTwoOrFour), undefined);
      assert.sameValue(findIndexHelper(fixedLength, isTwoOrFour), -1);
      assert.sameValue(findLastHelper(fixedLength, isTwoOrFour), undefined);
      assert.sameValue(findLastIndexHelper(fixedLength, isTwoOrFour), -1);
      assert.sameValue(findHelper(fixedLengthWithOffset, isTwoOrFour), undefined);
      assert.sameValue(findIndexHelper(fixedLengthWithOffset, isTwoOrFour), -1);
      assert.sameValue(findLastHelper(fixedLengthWithOffset, isTwoOrFour), undefined);
      assert.sameValue(findLastIndexHelper(fixedLengthWithOffset, isTwoOrFour), -1);
    }
    assert.sameValue(Number(findHelper(lengthTracking, isTwoOrFour)), 2);
    assert.sameValue(Number(findHelper(lengthTrackingWithOffset, isTwoOrFour)), 4);
    assert.sameValue(findIndexHelper(lengthTracking, isTwoOrFour), 1);
    assert.sameValue(findIndexHelper(lengthTrackingWithOffset, isTwoOrFour), 0);
    assert.sameValue(Number(findLastHelper(lengthTracking, isTwoOrFour)), 4);
    assert.sameValue(Number(findLastHelper(lengthTrackingWithOffset, isTwoOrFour)), 4);
    assert.sameValue(findLastIndexHelper(lengthTracking, isTwoOrFour), 2);
    assert.sameValue(findLastIndexHelper(lengthTrackingWithOffset, isTwoOrFour), 0);

    // Shrink so that the TAs with offset go out of bounds.
    rab.resize(1 * ctor.BYTES_PER_ELEMENT);
    if (oobThrows) {
      assert.throws(TypeError, () => {
        findHelper(fixedLength, isTwoOrFour);
      });
      assert.throws(TypeError, () => {
        findIndexHelper(fixedLength, isTwoOrFour);
      });
      assert.throws(TypeError, () => {
        findLastHelper(fixedLength, isTwoOrFour);
      });
      assert.throws(TypeError, () => {
        findLastIndexHelper(fixedLength, isTwoOrFour);
      });
      assert.throws(TypeError, () => {
        findHelper(fixedLengthWithOffset, isTwoOrFour);
      });
      assert.throws(TypeError, () => {
        findIndexHelper(fixedLengthWithOffset, isTwoOrFour);
      });
      assert.throws(TypeError, () => {
        findLastHelper(fixedLengthWithOffset, isTwoOrFour);
      });
      assert.throws(TypeError, () => {
        findLastIndexHelper(fixedLengthWithOffset, isTwoOrFour);
      });
      assert.throws(TypeError, () => {
        findHelper(lengthTrackingWithOffset, isTwoOrFour);
      });
      assert.throws(TypeError, () => {
        findIndexHelper(lengthTrackingWithOffset, isTwoOrFour);
      });
      assert.throws(TypeError, () => {
        findLastHelper(lengthTrackingWithOffset, isTwoOrFour);
      });
      assert.throws(TypeError, () => {
        findLastIndexHelper(lengthTrackingWithOffset, isTwoOrFour);
      });
    } else {
      assert.sameValue(findHelper(fixedLength, isTwoOrFour), undefined);
      assert.sameValue(findIndexHelper(fixedLength, isTwoOrFour), -1);
      assert.sameValue(findLastHelper(fixedLength, isTwoOrFour), undefined);
      assert.sameValue(findLastIndexHelper(fixedLength, isTwoOrFour), -1);
      assert.sameValue(findHelper(fixedLengthWithOffset, isTwoOrFour), undefined);
      assert.sameValue(findIndexHelper(fixedLengthWithOffset, isTwoOrFour), -1);
      assert.sameValue(findLastHelper(fixedLengthWithOffset, isTwoOrFour), undefined);
      assert.sameValue(findLastIndexHelper(fixedLengthWithOffset, isTwoOrFour), -1);
      assert.sameValue(findHelper(lengthTrackingWithOffset, isTwoOrFour), undefined);
      assert.sameValue(findIndexHelper(lengthTrackingWithOffset, isTwoOrFour), -1);
      assert.sameValue(findLastHelper(lengthTrackingWithOffset, isTwoOrFour), undefined);
      assert.sameValue(findLastIndexHelper(lengthTrackingWithOffset, isTwoOrFour), -1);
    }
    assert.sameValue(findHelper(lengthTracking, isTwoOrFour), undefined);
    assert.sameValue(findIndexHelper(lengthTracking, isTwoOrFour), -1);
    assert.sameValue(findLastHelper(lengthTracking, isTwoOrFour), undefined);
    assert.sameValue(findLastIndexHelper(lengthTracking, isTwoOrFour), -1);

    // Shrink to zero.
    rab.resize(0);
    if (oobThrows) {
      assert.throws(TypeError, () => {
        findHelper(fixedLength, isTwoOrFour);
      });
      assert.throws(TypeError, () => {
        findIndexHelper(fixedLength, isTwoOrFour);
      });
      assert.throws(TypeError, () => {
        findLastHelper(fixedLength, isTwoOrFour);
      });
      assert.throws(TypeError, () => {
        findLastIndexHelper(fixedLength, isTwoOrFour);
      });
      assert.throws(TypeError, () => {
        findHelper(fixedLengthWithOffset, isTwoOrFour);
      });
      assert.throws(TypeError, () => {
        findIndexHelper(fixedLengthWithOffset, isTwoOrFour);
      });
      assert.throws(TypeError, () => {
        findLastHelper(fixedLengthWithOffset, isTwoOrFour);
      });
      assert.throws(TypeError, () => {
        findLastIndexHelper(fixedLengthWithOffset, isTwoOrFour);
      });
      assert.throws(TypeError, () => {
        findHelper(lengthTrackingWithOffset, isTwoOrFour);
      });
      assert.throws(TypeError, () => {
        findIndexHelper(lengthTrackingWithOffset, isTwoOrFour);
      });
      assert.throws(TypeError, () => {
        findLastHelper(lengthTrackingWithOffset, isTwoOrFour);
      });
      assert.throws(TypeError, () => {
        findLastIndexHelper(lengthTrackingWithOffset, isTwoOrFour);
      });
    } else {
      assert.sameValue(findHelper(fixedLength, isTwoOrFour), undefined);
      assert.sameValue(findIndexHelper(fixedLength, isTwoOrFour), -1);
      assert.sameValue(findLastHelper(fixedLength, isTwoOrFour), undefined);
      assert.sameValue(findLastIndexHelper(fixedLength, isTwoOrFour), -1);
      assert.sameValue(findHelper(fixedLengthWithOffset, isTwoOrFour), undefined);
      assert.sameValue(findIndexHelper(fixedLengthWithOffset, isTwoOrFour), -1);
      assert.sameValue(findLastHelper(fixedLengthWithOffset, isTwoOrFour), undefined);
      assert.sameValue(findLastIndexHelper(fixedLengthWithOffset, isTwoOrFour), -1);
      assert.sameValue(findHelper(lengthTrackingWithOffset, isTwoOrFour), undefined);
      assert.sameValue(findIndexHelper(lengthTrackingWithOffset, isTwoOrFour), -1);
      assert.sameValue(findLastHelper(lengthTrackingWithOffset, isTwoOrFour), undefined);
      assert.sameValue(findLastIndexHelper(lengthTrackingWithOffset, isTwoOrFour), -1);
    }
    assert.sameValue(findHelper(lengthTracking, isTwoOrFour), undefined);
    assert.sameValue(findIndexHelper(lengthTracking, isTwoOrFour), -1);
    assert.sameValue(findLastHelper(lengthTracking, isTwoOrFour), undefined);
    assert.sameValue(findLastIndexHelper(lengthTracking, isTwoOrFour), -1);

    // Grow so that all TAs are back in-bounds.
    rab.resize(6 * ctor.BYTES_PER_ELEMENT);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, 0);
    }
    WriteToTypedArray(taWrite, 4, 2);
    WriteToTypedArray(taWrite, 5, 4);

    // Orig. array: [0, 0, 0, 0, 2, 4]
    //              [0, 0, 0, 0] << fixedLength
    //                    [0, 0] << fixedLengthWithOffset
    //              [0, 0, 0, 0, 2, 4, ...] << lengthTracking
    //                    [0, 0, 2, 4, ...] << lengthTrackingWithOffset

    assert.sameValue(findHelper(fixedLength, isTwoOrFour), undefined);
    assert.sameValue(findHelper(fixedLengthWithOffset, isTwoOrFour), undefined);
    assert.sameValue(Number(findHelper(lengthTracking, isTwoOrFour)), 2);
    assert.sameValue(Number(findHelper(lengthTrackingWithOffset, isTwoOrFour)), 2);
    assert.sameValue(findIndexHelper(fixedLength, isTwoOrFour), -1);
    assert.sameValue(findIndexHelper(fixedLengthWithOffset, isTwoOrFour), -1);
    assert.sameValue(findIndexHelper(lengthTracking, isTwoOrFour), 4);
    assert.sameValue(findIndexHelper(lengthTrackingWithOffset, isTwoOrFour), 2);
    assert.sameValue(findLastHelper(fixedLength, isTwoOrFour), undefined);
    assert.sameValue(findLastHelper(fixedLengthWithOffset, isTwoOrFour), undefined);
    assert.sameValue(Number(findLastHelper(lengthTracking, isTwoOrFour)), 4);
    assert.sameValue(Number(findLastHelper(lengthTrackingWithOffset, isTwoOrFour)), 4);
    assert.sameValue(findLastIndexHelper(fixedLength, isTwoOrFour), -1);
    assert.sameValue(findLastIndexHelper(fixedLengthWithOffset, isTwoOrFour), -1);
    assert.sameValue(findLastIndexHelper(lengthTracking, isTwoOrFour), 5);
    assert.sameValue(findLastIndexHelper(lengthTrackingWithOffset, isTwoOrFour), 3);
  }
}

FindFindIndexFindLastFindLastIndex(TypedArrayFindHelper, TypedArrayFindIndexHelper, TypedArrayFindLastHelper, TypedArrayFindLastIndexHelper, true);
FindFindIndexFindLastFindLastIndex(ArrayFindHelper, ArrayFindIndexHelper, ArrayFindLastHelper, ArrayFindLastIndexHelper, false);
