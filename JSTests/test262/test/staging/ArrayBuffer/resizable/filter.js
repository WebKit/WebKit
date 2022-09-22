// Copyright 2021 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-arraybuffer-length
description: >
  Automatically ported from Filter test
  in V8's mjsunit test typedarray-resizablearraybuffer.js
includes: [compareArray.js]
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

function ToNumbers(array) {
  let result = [];
  for (let item of array) {
    result.push(Convert(item));
  }
  return result;
}

const TypedArrayFilterHelper = (ta, ...rest) => {
  return ta.filter(...rest);
};

const ArrayFilterHelper = (ta, ...rest) => {
  return Array.prototype.filter.call(ta, ...rest);
};

function Filter(filterHelper, oobThrows) {
  for (let ctor of ctors) {
    const rab = CreateResizableArrayBuffer(4 * ctor.BYTES_PER_ELEMENT, 8 * ctor.BYTES_PER_ELEMENT);
    const fixedLength = new ctor(rab, 0, 4);
    const fixedLengthWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT, 2);
    const lengthTracking = new ctor(rab, 0);
    const lengthTrackingWithOffset = new ctor(rab, 2 * ctor.BYTES_PER_ELEMENT);

    // Write some data into the array.
    const taWrite = new ctor(rab);
    for (let i = 0; i < 4; ++i) {
      WriteToTypedArray(taWrite, i, i);
    }

    // Orig. array: [0, 1, 2, 3]
    //              [0, 1, 2, 3] << fixedLength
    //                    [2, 3] << fixedLengthWithOffset
    //              [0, 1, 2, 3, ...] << lengthTracking
    //                    [2, 3, ...] << lengthTrackingWithOffset

    function isEven(n) {
      return n != undefined && Number(n) % 2 == 0;
    }
    assert.compareArray(ToNumbers(filterHelper(fixedLength, isEven)), [
      0,
      2
    ]);
    assert.compareArray(ToNumbers(filterHelper(fixedLengthWithOffset, isEven)), [2]);
    assert.compareArray(ToNumbers(filterHelper(lengthTracking, isEven)), [
      0,
      2
    ]);
    assert.compareArray(ToNumbers(filterHelper(lengthTrackingWithOffset, isEven)), [2]);

    // Shrink so that fixed length TAs go out of bounds.
    rab.resize(3 * ctor.BYTES_PER_ELEMENT);

    // Orig. array: [0, 1, 2]
    //              [0, 1, 2, ...] << lengthTracking
    //                    [2, ...] << lengthTrackingWithOffset

    if (oobThrows) {
      assert.throws(TypeError, () => {
        filterHelper(fixedLength, isEven);
      });
      assert.throws(TypeError, () => {
        filterHelper(fixedLengthWithOffset, isEven);
      });
    } else {
      assert.compareArray(filterHelper(fixedLength, isEven), []);
      assert.compareArray(filterHelper(fixedLengthWithOffset, isEven), []);
    }
    assert.compareArray(ToNumbers(filterHelper(lengthTracking, isEven)), [
      0,
      2
    ]);
    assert.compareArray(ToNumbers(filterHelper(lengthTrackingWithOffset, isEven)), [2]);

    // Shrink so that the TAs with offset go out of bounds.
    rab.resize(1 * ctor.BYTES_PER_ELEMENT);
    if (oobThrows) {
      assert.throws(TypeError, () => {
        filterHelper(fixedLength, isEven);
      });
      assert.throws(TypeError, () => {
        filterHelper(fixedLengthWithOffset, isEven);
      });
      assert.throws(TypeError, () => {
        filterHelper(lengthTrackingWithOffset, isEven);
      });
    } else {
      assert.compareArray(filterHelper(fixedLength, isEven), []);
      assert.compareArray(filterHelper(fixedLengthWithOffset, isEven), []);
      assert.compareArray(filterHelper(lengthTrackingWithOffset, isEven), []);
    }
    assert.compareArray(ToNumbers(filterHelper(lengthTracking, isEven)), [0]);

    // Shrink to zero.
    rab.resize(0);
    if (oobThrows) {
      assert.throws(TypeError, () => {
        filterHelper(fixedLength, isEven);
      });
      assert.throws(TypeError, () => {
        filterHelper(fixedLengthWithOffset, isEven);
      });
      assert.throws(TypeError, () => {
        filterHelper(lengthTrackingWithOffset, isEven);
      });
    } else {
      assert.compareArray(filterHelper(fixedLength, isEven), []);
      assert.compareArray(filterHelper(fixedLengthWithOffset, isEven), []);
      assert.compareArray(filterHelper(lengthTrackingWithOffset, isEven), []);
    }
    assert.compareArray(ToNumbers(filterHelper(lengthTracking, isEven)), []);

    // Grow so that all TAs are back in-bounds.
    rab.resize(6 * ctor.BYTES_PER_ELEMENT);
    for (let i = 0; i < 6; ++i) {
      WriteToTypedArray(taWrite, i, i);
    }

    // Orig. array: [0, 1, 2, 3, 4, 5]
    //              [0, 1, 2, 3] << fixedLength
    //                    [2, 3] << fixedLengthWithOffset
    //              [0, 1, 2, 3, 4, 5, ...] << lengthTracking
    //                    [2, 3, 4, 5, ...] << lengthTrackingWithOffset

    assert.compareArray(ToNumbers(filterHelper(fixedLength, isEven)), [
      0,
      2
    ]);
    assert.compareArray(ToNumbers(filterHelper(fixedLengthWithOffset, isEven)), [2]);
    assert.compareArray(ToNumbers(filterHelper(lengthTracking, isEven)), [
      0,
      2,
      4
    ]);
    assert.compareArray(ToNumbers(filterHelper(lengthTrackingWithOffset, isEven)), [
      2,
      4
    ]);
  }
}

Filter(TypedArrayFilterHelper, true);
Filter(ArrayFilterHelper, false);
